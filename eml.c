#include "eml.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// #define EML_PARSER_VERSION "0.0.0"
// #define DEBUG

// The maximum length a user-input string may be (excluding sentinel)
#define MAX_NAME_LENGTH 128
#define MAX_VERSION_STRING_LENGTH 12
#define MAX_WEIGHT_UNIT_STRING_LENGTH 3

#define MAX_FORMATTED_EML_STRING_LENGTH 12

#define true 1
#define false 0
#define right 1
#define left 0

/* 
 * Parser Parameters:
 * version - the version in the eml header
 * weightUnit - the weight abbreviation in the eml header
 */ 
static char version[MAX_VERSION_STRING_LENGTH + 1];
static char weightUnit[MAX_WEIGHT_UNIT_STRING_LENGTH + 1];

/*
 * Global Parser Vars:
 * emlString - The eml to be parsed
 * emlstringlen - The length of the emlString (excluding sentinel)
 * current_position - The index of emlString the parser is currently on
 */
static char *emlString;
static int emlstringlen;
static int current_postition;

static int parse_header(eml_result *result);

static void validate_header_t(eml_header_t *h);

static int parse_string(char **result);

static int parse_header_t(eml_header_t **tht);
static int parse_super_t(eml_super_t **tsupt);
static int parse_single_t(eml_single_t **tst);

static int applying_reps_type(eml_reps *r, uint32_t t);
static int flush(eml_single_t *tst, uint32_t *vcount, eml_kind_flag kind, eml_modifier_flag mod, eml_number *buf, uint32_t *dcount);
static void move_to_asymmetric(eml_single_t *tst, bool side);
static int upgrade_to_asymmetric(eml_single_t *tst);
static int upgrade_to_standard_varied(eml_single_t *tst);
static int upgrade_to_standard(eml_single_t *tst);

static void format_eml_number(eml_number *e, char *f);
static void print_standard_k(eml_standard_k *k);
static void print_standard_varied_k(eml_standard_varied_k *k);
static void print_single_t(eml_single_t *s);
static void print_super_t(eml_super_t *s);
static void print_emlobj(eml_obj *e);

static void free_single_t(eml_single_t *s);
static void free_super_t(eml_super_t *s);
static void free_emlobj(eml_obj *e);

/*
 * parse: Entry point for parsing eml. Starts at '{', ends at (emlstringlen - 1)
 */
int parse(char *eml_string, eml_result **result) {
    emlString = eml_string;
    emlstringlen = strlen(eml_string);
    current_postition = 0;

    version[0] = 0;
    weightUnit[0] = 0;

    *result = malloc(sizeof(eml_result));
    if (*result == NULL) {
        return allocation_error;
    }

    (*result)->header = NULL;
    (*result)->objs = NULL;

    eml_obj *obj_tail = NULL;

    #ifdef DEBUG
        printf("EML String: %s, length: %i\n", eml_string, emlstringlen);
    #endif

    eml_super_t *tsupt = NULL;
    eml_single_t *tst = NULL;
    int error = 0;

    while (current_postition < emlstringlen) {
        char current = emlString[current_postition];

        switch (current) {
        case (int)'{': // Give control to parse_header()
            parse_header(*result);

            if (version[0] == '\0') {
                error = missing_version;
                goto bail;
            }

            if (weightUnit[0] == '\0') {
                error = missing_weight_unit;
                goto bail;
            }

            #ifdef DEBUG
                printf("parsed version: %s, parsed weight: %s\n", version, weightUnit);
                printf("-------------------------\n");
            #endif

            break;
        case (int)'s': // Give control to parse_super_t()
            if ((error = parse_super_t(&tsupt))) {
                goto bail;
            }

            eml_obj *temp_super = malloc(sizeof(eml_obj));
            if (temp_super == NULL) {
                error = allocation_error;
                goto bail;
            }
            temp_super->type = super;
            temp_super->data = tsupt;
            temp_super->next = NULL;

            if ((*result)->objs == NULL) {
                (*result)->objs = temp_super;
            } else {
                obj_tail->next = temp_super;
            }

            obj_tail = temp_super;
            break;
        case (int)'c': // Give control to parse_super_t()
            if ((error = parse_super_t(&tsupt))) {
                goto bail;
            }

            eml_obj *temp_circuit = malloc(sizeof(eml_obj));
            if (temp_circuit == NULL) {
                error = allocation_error;
                goto bail;
            }
            temp_circuit->type = circuit;
            temp_circuit->data = tsupt;
            temp_circuit->next = NULL;

            if ((*result)->objs == NULL) {
                (*result)->objs = temp_circuit;
            } else {
                obj_tail->next = temp_circuit;
            }

            obj_tail = temp_circuit;
            break;
        case (int)'\"': // Give control to parse_single_t() 
            if ((error = parse_single_t(&tst))) {
                goto bail;
            }

            eml_obj *temp_single = malloc(sizeof(eml_obj));
            if (temp_single == NULL) {
                error = allocation_error;
                goto bail;
            }
            temp_single->type = single;
            temp_single->data = tst;
            temp_single->next = NULL;

            if ((*result)->objs == NULL) {
                (*result)->objs = temp_single;
            } else {
                obj_tail->next = temp_single;
            }

            obj_tail = temp_single;
            break;
        case (int)';':
            ++current_postition;
            break;
        default:
            error = unexpected_error;
            goto bail;
        }
    }

    return no_error;

    bail:
        if (tst != NULL) {
            free_single_t(tst);
        }

        if (tsupt != NULL) {
            free_super_t(tsupt);
        }

        free_result(*result);
        return error;
}

/*
 * parse_header: Parses header section or exits. Starts on "{" of header, ends on char succeeding "}"
*/
static int parse_header(eml_result *result) {
    eml_header_t *tht = NULL;
    int error = no_error;

    if (emlString[current_postition++] != (int)'{') {
        error = missing_header_start_char;
        return error;
    }

    while (current_postition < emlstringlen) {
        char current = emlString[current_postition];

        switch (current){
        case (int)'}': // Release control & inc
            ++current_postition;
            return no_error;
        case (int)',':
            ++current_postition;
            break;
        case (int)'\"':
            if ((error = parse_header_t(&tht))) {
                return error;
            }
            validate_header_t(tht);
            tht->next = result->header;
            result->header = tht;
            break;
        default:
            return unexpected_error;
        }
    }

    error = unexpected_error;

    return error;
}

/*
 * parse_header_t: Returns an eml_header_t or exits. Starts on '"', ends on ',' or '}'.
*/
static int parse_header_t(eml_header_t **tht) {
    *tht = malloc(sizeof(eml_header_t));
    if (*tht == NULL) {
        return allocation_error;
    }
    
    bool pv = false; // Toggle between parameter & value

    int error = no_error;

    while (current_postition < emlstringlen) {
        char current = emlString[current_postition];

        switch (current) {
            case (int)'}': // Release control
                return no_error;
            case (int)',': // Release control
                return no_error;
            case (int)':':
                pv = true;
                ++current_postition;
                break;
            case (int)'\"':
                if (pv == false) {
                    if ((error = parse_string(&(*tht)->parameter))) {
                        goto bail;
                    }
                }
                else {
                    if ((error = parse_string(&(*tht)->value))) {
                        goto bail;
                    }
                }
                break;
            default:
                error = unexpected_error;
                goto bail;
        }
    }

    error = unexpected_error;

    bail:
        if (*tht != NULL) {
            if ((*tht)->parameter != NULL) {
                free((*tht)->parameter);
            }

            if ((*tht)->value != NULL) {
                free((*tht)->value);
            }

            free(*tht);
        }

        return error;
}

/*
 * parse_super_t: Returns an eml_super_t or exits. Starts on 's', ends succeeding ')'.
*/
static int parse_super_t(eml_super_t **tsupt) {
    *tsupt = malloc(sizeof(eml_super_t));
    if (tsupt == NULL) {
        return allocation_error;
    } 

    eml_single_t *tst = NULL;
    eml_super_member_t *set_tail = NULL;

    int error = no_error;

    while (current_postition < emlstringlen) {
        char current = emlString[current_postition];

        switch (current) {
            case (int)'(':
                ++current_postition;
                break;
            case (int)'\"':
                if ((error = parse_single_t(&tst))) {
                    return error;
                }

                eml_super_member_t *temp = malloc(sizeof(eml_super_member_t));
                temp->single = tst;
                temp->next = NULL;

                if ((*tsupt)->sets == NULL) {
                    (*tsupt)->sets = temp;
                } else {
                    set_tail->next = temp;
                }

                (*tsupt)->count++;
                set_tail = temp;
                break;
            case (int)')':
                ++current_postition;
                return no_error;
            default: // skip {'u', 'p', 'e', 'r'} and {'i', 'r', 'c', 'u', 'i', 't'}
                ++current_postition;
                break;
        }
    }

    error = unexpected_error;

    return error;
}

/*
 * parse_single_t: Returns an eml_single_t or exits. Starts on '"', ends succeeding ';'
*/
static int parse_single_t(eml_single_t **tst) {
    *tst = malloc(sizeof(eml_single_t));
    if (tst == NULL) {
        return allocation_error;
    } 

    // Initialize eml_single_t
    (*tst)->name = NULL;
    (*tst)->no_work = NULL;
    (*tst)->standard_work = NULL;
    (*tst)->standard_varied_work = NULL;
    (*tst)->asymmetric_work = NULL;

    int error = no_error;
    // #define BAIL(e) { error = e; goto bail;}

    if ((error = parse_string(&(*tst)->name))) {
        goto bail;
    }

    eml_kind_flag kind = none;
    eml_modifier_flag modifier = no_mod; 

    eml_number buffer_int = 0;  // Rolling eml_number
    uint32_t dcount = 0;        // If >0, writing to (dcount * 10)'ths place, >2 error
    uint32_t vcount = 0;        // Index of standard_varied_k.vReps[]

    uint32_t temp;              // Used for building eml_number in `default`

    if (emlString[current_postition++] != (int)':') {
        error = name_work_separator_error;
        goto bail;
    }

    while (current_postition < emlstringlen) {
        char current = emlString[current_postition];

        switch (current) {
        case (int)'\"':
            ++current_postition;
            break;
        case (int)':':
            // Upgrade to asymetric_k
            // Write value/modifier. If kind == standard_varied_work, write as macro.
            if ((error = flush(*tst, NULL, kind, modifier, &buffer_int, &dcount))) {
                goto bail;
            } 

            modifier = no_mod;
            kind = none;

            // Upgrade existing eml_single_t to asymmetric
            if ((error = upgrade_to_asymmetric(*tst))) {
                goto bail;
            }
            
            ++current_postition;
            break;
        case (int)'x':
            // Allocate standard_work & set sets.
            if ((error = upgrade_to_standard(*tst))) {
                goto bail;
            }
            (*tst)->standard_work->sets = buffer_int;

            buffer_int = 0;
            kind = standard;

            ++current_postition;
            break;
        case (int)'(':
            // Allocate standard_varied_work & dealloc/transition standard_work
            if ((error = upgrade_to_standard_varied(*tst))) {
                goto bail;
            }

            vcount = 0;
            kind = standard_varied;

            ++current_postition;
            break;
        case (int)',':
            if (vcount > (*tst)->standard_varied_work->sets) {
                error = extra_variable_reps_error;
                goto bail;
            }

            // Write reps/(internal)modifiers
            if ((error = flush(*tst, &vcount, kind, modifier, &buffer_int, &dcount))) {
                goto bail;
            }

            vcount++;
            modifier = no_mod;

            ++current_postition;
            break;
        case (int)')':
            if (vcount > (*tst)->standard_varied_work->sets) {
                error = extra_variable_reps_error;
                goto bail;
            }

            // Write reps/(internal)modifiers
            if ((error = flush(*tst, &vcount, kind, modifier, &buffer_int, &dcount))) {
                goto bail;
            }

            vcount++;
            modifier = no_mod;

            if (vcount < (*tst)->standard_varied_work->sets) {
                error = missing_variable_reps_error;
                goto bail;
            }
            ++current_postition;
            break;
        case (int)'F':
            switch (kind) {
                case none:
                    error = none_work_to_failure_error;
                    goto bail;
                case standard:
                    if ((error = applying_reps_type(&(*tst)->standard_work->reps, unmodifiedFailure))) {
                        goto bail;
                    }
                    break;
                case standard_varied:
                    // Apply 'toFailure' to internal Reps
                    if (vcount < (*tst)->standard_varied_work->sets) {
                        if ((error = applying_reps_type(&(*tst)->standard_varied_work->vReps[vcount], unmodifiedFailure))) {
                            goto bail;
                        }
                    }
                    else {
                        error = to_failure_used_as_macro_error;
                        goto bail;
                    }
                    break;
            }

            ++current_postition;
            break;
        case (int)'T':
            switch (kind) {
                case none:
                    error = modifier_on_none_work_error;
                    goto bail;
                case standard:
                    if ((error = applying_reps_type(&(*tst)->standard_work->reps, unmodifiedTime))) {
                        goto bail;
                    }
                    break;
                case standard_varied:
                    // Apply 'isTime' to internal Reps
                    if (vcount < (*tst)->standard_varied_work->sets) {
                        if ((error = applying_reps_type(&(*tst)->standard_varied_work->vReps[vcount], unmodifiedTime))) {
                            goto bail;
                        }
                    }
                    else {
                        error = time_macro_error;
                        goto bail;
                    }
                    break;
            }

            ++current_postition;
            break;
        case (int)'@':
            switch (kind) {
                case none:
                    error = modifier_on_none_work_error;
                    goto bail;
                case standard:
                    (*tst)->standard_work->reps.value = buffer_int;
                    break;
                case standard_varied:
                    // Apply weight modifier to internal Reps (EX: 3x(5@120,...,...))
                    if (vcount < (*tst)->standard_varied_work->sets) {
                        (*tst)->standard_varied_work->vReps[vcount].value = buffer_int;
                    }
                    break;
            }

            buffer_int = 0;
            dcount = 0;
            modifier = weight_mod;
            
            ++current_postition;
            break;
        case (int)'%':
            switch (kind) {
                case none:
                    error = modifier_on_none_work_error;
                    goto bail;
                case standard:
                    (*tst)->standard_work->reps.value = buffer_int;
                    break;
                case standard_varied:
                    // Apply weight modifier to internal Reps (EX: 3x(5%120,...,...))
                    if (vcount < (*tst)->standard_varied_work->sets) {
                        (*tst)->standard_varied_work->vReps[vcount].value = buffer_int;
                    }
                    break;
            }

            buffer_int = 0;
            dcount = 0;
            modifier = rpe_mod;
            
            ++current_postition;
            break;
        case (int)'.':
            if (kind == none) {
                error = fractional_sets_error;
                goto bail;
            }

            if (dcount) {
                error = multiple_radix_points_error;
                goto bail;
            }

            if (modifier == no_mod) {
                error = fractional_none_modifier_value_error;
                goto bail;
            }

            // Set H bit, potential overflow handled in `default`
            buffer_int = buffer_int * 100U | eml_number_H; 

            ++dcount;
            ++current_postition;
            break;
        case (int)';':
            // Write value/modifier. If kind == standard_varied_work, write as macro.
            if ((error = flush(*tst, NULL, kind, modifier, &buffer_int, &dcount))) {
                goto bail;
            }

            if ((*tst)->asymmetric_work != NULL) {
                move_to_asymmetric(*tst, right);
            }

            ++current_postition;
            return no_error; // Give control back
        default:
            switch (dcount) {
                case 0: // Before radix
                    temp = buffer_int * 10U + (unsigned int)current - '0';

                    if (temp > 21474836U) { 
                        error = integral_overflow_error;
                        goto bail;
                    }

                    buffer_int = temp;
                    break;
                case 1: // 10ths place
                    temp = buffer_int + ((unsigned int)current - '0') * 10U;

                    if ((temp & eml_number_mask) > 2147483647U) {
                        error = fp_overflow_error;
                        goto bail;
                    }

                    buffer_int = temp;
                    dcount++;
                    break;
                case 2: // 100ths place
                    temp = buffer_int + ((unsigned int)current - '0');

                    if ((temp & eml_number_mask) > 2147483647U) {
                        error = fp_overflow_error;
                        goto bail;
                    }

                    buffer_int = temp;
                    dcount++;
                    break;
                default:
                    error = too_many_fp_digits;
                    goto bail;
            }
            
            ++current_postition;
            break;
        }
    }

    error = unexpected_error;

    bail:
        return error;
}

/*
 * validate_header_t: Checks and adds parser configuration from eml_header_t
 */
static void validate_header_t(eml_header_t *h) {
    if (version[0] == '\0' && strcmp(h->parameter, "version") == 0) {
        strncpy(version, h->value, MAX_VERSION_STRING_LENGTH);
        version[MAX_VERSION_STRING_LENGTH] = '\0';
    } else if (weightUnit[0] == '\0' && strcmp(h->parameter, "weight") == 0) {
        strncpy(weightUnit, h->value, MAX_WEIGHT_UNIT_STRING_LENGTH);
        weightUnit[MAX_WEIGHT_UNIT_STRING_LENGTH] = '\0';
    }
}

/*
 * parse_string: Returns a string (char*) or exits. Starts on '"', ends succeeding the next '"'
 */
static int parse_string(char **result) {
    static char strbuf[MAX_NAME_LENGTH + 1];
    uint32_t strindex = 0;

    ++current_postition; // skip '"'
    while (current_postition < emlstringlen) {
        char current = emlString[current_postition];

        switch (current) {
            case (int)'\"':
                ++current_postition;

                if (strindex == 0) {
                    return empty_string_error;
                }

                *result = malloc(strindex + 1);
                if (*result == NULL) {
                    return allocation_error;
                }
            
                for(int i = 0; i < strindex; i++) {
                    (*result)[i] = strbuf[i];
                }
                (*result)[strindex] = '\0';
                return no_error;
            default:
                if (strindex > 127) {
                    free(*result);
                    return string_length_error;
                }

                strbuf[strindex++] = current;
                ++current_postition;
                break;
        }
    }

    return no_error;
}

/*
 * applying_reps_type: Transitions REPS type to new type
 */
static int applying_reps_type(eml_reps *r, uint32_t t) {
    switch (r->type) {
        case unmodified:
            switch (t) {
                case unmodifiedFailure:
                    r->type = unmodifiedFailure;
                    break;
                case unmodifiedTime:
                    r->type = unmodifiedTime;
                    break;
                case weight:
                    r->type = weight;
                    break;
                case rpe:
                    r->type = rpe;
                    break;
                default:
                    return bad_reps_type_transition;
            }

            break;
        case unmodifiedFailure:
            switch (t) {
                case unmodifiedTime:
                    r->type = unmodifiedTimeFailure;
                    break;
                case weight:
                    r->type = weightFailure;
                    break;
                case rpe:
                    return rpe_to_failure;
                default:
                    return bad_reps_type_transition;
            }

            break;
        case unmodifiedTime:
            switch (t) {
                case unmodifiedFailure:
                    r->type = unmodifiedTimeFailure;
                    break;
                case weight:
                    r->type = timeWeight;
                    break;
                case rpe:
                    r->type = timeRPE;
                    break;
                default:
                    return bad_reps_type_transition;
            }

            break;
        case unmodifiedTimeFailure:
            switch (t) {
                case weight:
                    r->type = timeWeightFaliure;
                    break;
                case rpe:
                    return rpe_to_failure;
                default:
                    return bad_reps_type_transition;
            }

            break;
        default:
            return bad_reps_type_transition;
    }

    return no_error;
}

/* 
 * flush: Writes buf to the appropriate field in eml_single_t. If there is none work, flush will malloc tst->no_work. 
 *        If `vcount` is NULL, modifier will be applied as a macro. Resets `buf` and `dcount`.
 */
static int flush(eml_single_t *tst, uint32_t *vcount, eml_kind_flag kind, eml_modifier_flag mod, eml_number *buf, uint32_t *dcount) {
    int error = no_error;
    if (*dcount == 1) {
        return missing_digit_following_radix_error;
    }

    switch (kind) {
        case none:
            switch (mod) {
                case no_mod:
                    tst->no_work = malloc(sizeof(bool));
                    if (tst->no_work == NULL) {
                        return allocation_error;
                    }

                    break;
                default:
                    return modifier_on_none_work_error;
            }
            break;
        case standard:
            switch (mod) {
                case no_mod:
                    tst->standard_work->reps.value = *buf; 
                    break;
                case weight_mod:
                    if ((error = applying_reps_type(&tst->standard_work->reps, weight))) {
                        return error;
                    }
                    tst->standard_work->reps.modifier.weight = *buf;
                    break;
                case rpe_mod:
                    if ((error = applying_reps_type(&tst->standard_work->reps, rpe))) {
                        return error;
                    }
                    tst->standard_work->reps.modifier.rpe = *buf;
                    break;
            }
            break;
        case standard_varied:
            if (vcount == NULL) { // MACRO MODIFIER
                switch (mod) {
                    case no_mod:
                        break;
                    case weight_mod:
                        for (int i = 0; i < tst->standard_varied_work->sets; i++) {
                            // Only apply weight macro to reps which do not have a modifier
                            switch (tst->standard_varied_work->vReps[i].type) {
                                case unmodified:
                                case unmodifiedFailure:
                                case unmodifiedTime:
                                case unmodifiedTimeFailure:
                                    if ((error = applying_reps_type(&tst->standard_varied_work->vReps[i], weight))) {
                                        return error;
                                    }
                                    tst->standard_varied_work->vReps[i].modifier.weight = *buf;
                                    break;
                                default:
                                    break;
                            }
                        }
                        break;
                    case rpe_mod:
                        for (int i = 0; i < tst->standard_varied_work->sets; i++) {
                            // Only apply RPE macro to reps which do not have a modifier.
                            switch (tst->standard_varied_work->vReps[i].type) {
                                case unmodified:
                                case unmodifiedTime:
                                    if ((error = applying_reps_type(&tst->standard_varied_work->vReps[i], rpe))) {
                                        return error;
                                    }
                                    tst->standard_varied_work->vReps[i].modifier.rpe = *buf; // add validation
                                    break;
                                case unmodifiedFailure:
                                case unmodifiedTimeFailure:
                                    // Applying RPE as a macro to a failure set is an error
                                    return rpe_to_failure;
                                default:
                                    break;
                            }
                        }
                        break;
                }
            } else { // INNER-MODIFIER
                switch (mod) {
                    case no_mod:
                        tst->standard_varied_work->vReps[*vcount].value = *buf;
                        break;
                    case weight_mod:
                        if ((error = applying_reps_type(&tst->standard_varied_work->vReps[*vcount], weight))) {
                            return error;
                        }
                        tst->standard_varied_work->vReps[*vcount].modifier.weight = *buf;
                        break;
                    case rpe_mod:
                        if ((error = applying_reps_type(&tst->standard_varied_work->vReps[*vcount], rpe))) {
                            return error;
                        }
                        tst->standard_varied_work->vReps[*vcount].modifier.rpe = *buf;
                        break;
                }
            }
            break;
    }

    *buf = 0;
    *dcount = 0;
    return no_error;
}

/*
 * move_to_asymmetric: Moves tst->(no_work | standard_work | standard_varied_work) kind to the left or right side of tst->asymmetric_work.
 */
static void move_to_asymmetric(eml_single_t *tst, bool side) {
    if (tst->no_work != NULL) {
        if (side) {
            tst->asymmetric_work->right_none_k = tst->no_work;
        } else {
            tst->asymmetric_work->left_none_k = tst->no_work;
        }
        
        tst->no_work = NULL;
    }
    else if (tst->standard_work != NULL) {
        if (side) {
            tst->asymmetric_work->right_standard_k = tst->standard_work;
        } else {
            tst->asymmetric_work->left_standard_k = tst->standard_work;
        }

        tst->standard_work = NULL;
    }
    else if (tst->standard_varied_work != NULL) {
        if (side) {
            tst->asymmetric_work->right_standard_varied_k = tst->standard_varied_work;
        } else {
            tst->asymmetric_work->left_standard_varied_k = tst->standard_varied_work;
        }

        tst->standard_varied_work = NULL;
    }
}

/*
 * upgrade_to_asymmetric: Allocates tst->asymmetric_work & moves existing tst->(no_work | standard_work | standard_varied_work) kind to left side.
 */
static int upgrade_to_asymmetric(eml_single_t *tst) {
    tst->asymmetric_work = malloc(sizeof(eml_asymmetric_k));
    if (tst->asymmetric_work == NULL) {
        return allocation_error;
    }

    tst->asymmetric_work->left_none_k = NULL;
    tst->asymmetric_work->left_standard_k = NULL;
    tst->asymmetric_work->left_standard_varied_k = NULL;
    tst->asymmetric_work->right_none_k = NULL;
    tst->asymmetric_work->right_standard_k = NULL;
    tst->asymmetric_work->right_standard_varied_k = NULL;

    move_to_asymmetric(tst, left);
    return no_error;
}

/*
 * upgrade_to_standard_varied: Allocates tst->standard_varied_work & migrates tst->standard_work.
 */
static int upgrade_to_standard_varied(eml_single_t *tst) {
    tst->standard_varied_work = malloc(sizeof(eml_reps) * tst->standard_work->sets + sizeof(eml_number));
    if (tst->standard_varied_work == NULL) {
        return allocation_error;
    }
    
    tst->standard_varied_work->sets = tst->standard_work->sets;

    // standard_varied_kind defaults
    for (int i = 0; i < tst->standard_varied_work->sets; i++) {
        tst->standard_varied_work->vReps[i].type = unmodified;
    }

    free(tst->standard_work);
    tst->standard_work = NULL;
    return no_error;
}

/*
 * upgrade_to_standard: Allocates tst->standard_work.
 */
static int upgrade_to_standard(eml_single_t *tst) {
    tst->standard_work = malloc(sizeof(eml_standard_k));
    if (tst->standard_work == NULL) {
        return allocation_error;
    }

    tst->standard_work->reps.type = unmodified;
    return no_error;
}

/*
 * format_eml_number: Returns a temporary formatted eml_number string which is valid until next call or exits.
 */
static void format_eml_number(eml_number *e, char *f) {
    if (*e & eml_number_H) {
        uint32_t masked = (*e & eml_number_mask);
        sprintf(f, "%u.%u", masked / 100U, masked % 100U);
    } else {
        // <= 21474836U check ignored
        sprintf(f, "%u", *e);
    }
}

/*
 * print_standard_k: Prints an eml_standard_k to stdout.
 */
static void print_standard_k(eml_standard_k *k) {
    char value[MAX_FORMATTED_EML_STRING_LENGTH];
    char modifier[MAX_FORMATTED_EML_STRING_LENGTH];

    format_eml_number((eml_number *)&(k->reps.value), value);
    format_eml_number((eml_number *)&(k->reps.modifier), modifier);

    switch (k->reps.type) {
        case unmodified:
            printf("%i sets of %s reps", k->sets, value);
            break;
        case unmodifiedFailure:
            printf("%i sets to failure", k->sets);
            break;
        case unmodifiedTime:
            printf("%i time sets of %s seconds", k->sets, value);
            break;
        case unmodifiedTimeFailure:
            printf("%i time sets to failure", k->sets);
            break;
        case weight:
            printf("%i sets of %s reps with %s %s", k->sets, value, modifier, weightUnit);
            break;
        case weightFailure:
            printf("%i sets to failure with %s %s", k->sets, modifier, weightUnit);
            break;
        case timeWeight:
            printf("%i time sets of %s seconds with %s %s", k->sets, value, modifier, weightUnit);
            break;
        case timeWeightFaliure:
            printf("%i time sets to failure with %s %s", k->sets, modifier, weightUnit);
            break;
        case rpe:
            printf("%i sets of %s reps with RPE of %s", k->sets, value, modifier);
            break;
        case timeRPE:
            printf("%i time sets of %s seconds with RPE of %s", k->sets, value, modifier);
            break;
    }

    printf("\n");
}

/*
 * print_standard_varied_k: Prints an eml_standard_varied_k to stdout.
 */
static void print_standard_varied_k(eml_standard_varied_k *k) {
    int count = k->sets;
    printf("%i sets\n", count);
    for (int i = 0; i < count; i++) {
        printf(" - ");

        // standard_varied_k emulates standard_k for printing
        eml_standard_k shim;
        shim.sets = k->sets;
        shim.reps = k->vReps[i];
        print_standard_k(&shim); // FIXME: Prints X sets of ... on every line
    }
}

/*
 * print_single_t: Prints a eml_single_t to stdout.
 */
static void print_single_t(eml_single_t *s) {
    printf("--- single_t ---\n");
    printf("Name: %s\n", s->name);

    if (s->no_work != NULL) {
        printf("No work\n");
    } else if (s->standard_work != NULL) {
        printf("Standard work\n");
        print_standard_k(s->standard_work);
    } else if (s->standard_varied_work != NULL) {
        printf("Standard varied work\n");
        print_standard_varied_k(s->standard_varied_work);
    } else if (s->asymmetric_work != NULL) {
        printf("Asymetric work\n");

        if (s->asymmetric_work->left_none_k != NULL) {
            printf("LEFT: No work\n");
        } else if (s->asymmetric_work->left_standard_k != NULL) {
            printf("LEFT: Standard work ");
            print_standard_k(s->asymmetric_work->left_standard_k);
        } else if (s->asymmetric_work->left_standard_varied_k != NULL) {
            printf("LEFT: Standard varied work ");
            print_standard_varied_k(s->asymmetric_work->left_standard_varied_k);
        }

        if (s->asymmetric_work->right_none_k != NULL) {
            printf("RIGHT: No work\n");
        } else if (s->asymmetric_work->right_standard_k != NULL) {
            printf("RIGHT: Standard work ");
            print_standard_k(s->asymmetric_work->right_standard_k);
        } else if (s->asymmetric_work->right_standard_varied_k != NULL) {
            printf("RIGHT: Standard varied work ");
            print_standard_varied_k(s->asymmetric_work->right_standard_varied_k);
        }
    }
}

/*
 * print_super_t: Prints a eml_super_t to stdout.
 */
static void print_super_t(eml_super_t *s) {
    printf("----- SUPER -----\n");
    eml_super_member_t *current = s->sets;
    while(current != NULL) {
        print_single_t(current->single);
        current = current->next;
    }
    printf("--- SUPER END ---\n");
    return;
}

/*
 * print_circuit_t: Prints a eml_circuit_t to stdout.
 */
static void print_circuit_t(eml_circuit_t *c) {
    printf("----- CIRCUIT -----\n");
    eml_super_member_t *current = c->sets;
    while(current != NULL) {
        print_single_t(current->single);
        current = current->next;
    }
    printf("--- CIRCUIT END ---\n");
    return;
}

/*
 * print_emlobj: Prints an eml_obj to stdout.
 */
static void print_emlobj(eml_obj *e) {
    switch (e->type) {
        case single:
            print_single_t((eml_single_t*) e->data);
            break;
        case super:
            print_super_t((eml_super_t*) e->data);
            break;
        case circuit:
            print_circuit_t((eml_circuit_t*) e->data);
            break;
    }
}

void print_result(eml_result *result) {
    if (result == NULL) {
        return;
    }

    printf("--- Parsed EML ---\n");
    printf("Header:\n");

    eml_header_t *h = result->header;
    while (h != NULL) {
        printf(" - Parameter: %s, Value: %s\n", h->parameter, h->value);
        h = h->next;
    }

    printf("Body:\n");
    eml_obj *obj = result->objs;
    while(obj != NULL) {
        print_emlobj(obj);
        obj = obj->next;
    }
}

/*
 * free_single_t: Frees a eml_single_t.
 */
static void free_single_t(eml_single_t *s) {
    if (s->name != NULL) {
        free(s->name);
    }

    if (s->no_work != NULL) {
        free(s->no_work);
    }

    if (s->standard_work != NULL) {
        free(s->standard_work);
    }

    if (s->standard_varied_work != NULL) {
        free(s->standard_varied_work);
    }

    if (s->asymmetric_work != NULL) {
        if (s->asymmetric_work->left_none_k != NULL) {
            free(s->asymmetric_work->left_none_k);
        }

        if (s->asymmetric_work->left_standard_k != NULL) {
            free(s->asymmetric_work->left_standard_k);
        }

        if (s->asymmetric_work->left_standard_varied_k != NULL) {
            free(s->asymmetric_work->left_standard_varied_k);
        }

        if (s->asymmetric_work->right_none_k != NULL) {
            free(s->asymmetric_work->right_none_k);
        }

        if (s->asymmetric_work->right_standard_k != NULL) {
            free(s->asymmetric_work->right_standard_k);
        }

        if (s->asymmetric_work->right_standard_varied_k != NULL) {
            free(s->asymmetric_work->right_standard_varied_k);
        }

        free(s->asymmetric_work);
    }

    free(s);
}

/*
 * free_super_t: Frees a eml_super_t.
 */
static void free_super_t(eml_super_t *s) {
    eml_super_member_t *current = s->sets;
    while(current != NULL) {
        free_single_t(current->single);
        
        eml_super_member_t *t = current;
        current = current->next;
        free(t);
    }
    free(s);
}

/*
 * free_emlobj: Frees an eml_obj.
 */
static void free_emlobj(eml_obj *e) {
    if (e->type == single) {
        free_single_t((eml_single_t*) e->data);
    } else {
        free_super_t((eml_super_t*) e->data);
    }
}

/*
 * free_results: Frees an eml_result.
 */
void free_result(eml_result *result) {
    if (result == NULL) {
        return;
    }

    eml_header_t *h = result->header;
    while (h != NULL) {
        result->header = h->next;

        free(h->parameter);
        free(h->value);
        free(h);
        h = result->header;
    }

    eml_obj *obj = result->objs;
    while(obj != NULL) {
        result->objs = obj->next;
        free_emlobj(obj);
        obj = result->objs;
    }

    free(result);
    result = NULL;
}
