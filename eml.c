#include "eml.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// #define EML_PARSER_VERSION "0.0.0"
#define DEBUG

/*
 * The maximum length a user-input string may be (excluding sentinel)
 */
#define MAX_NAME_LENGTH 128

#define true 1
#define false 0

/* 
 * Parser Parameters:
 * version - the version in the eml header
 * weight - the weight abbreviation in the eml header
 */ 
static char version[13];
static char weight[4];

/*
 * Global Parser Vars:
 * emlString - The eml to be parsed
 * emlstringlen - The length of the emlString (excluding sentinel)
 * current_position - The index of emlString the parser is currently on
 */
static char *emlString;
static int emlstringlen;
static int current_postition;

/*
 * Parser Output:
 * result - Parsed eml objects
 * header - Linked List of header tokens
 */
static Array result;
static eml_header_t *header;

int parse(char *eml_string);
static int parse_header();

static void validate_header_t(eml_header_t *h);

static int parse_string(char **result);

static int parse_header_t(eml_header_t **tht);
static int parse_super_t(eml_super_t **tsupt);
static int parse_single_t(eml_single_t **tst);

static int flush(eml_single_t *tst, uint32_t *vcount, eml_kind_flag kind, eml_modifier_flag mod, eml_number *buf, uint32_t *dcount);
static void moveToAsymmetric(eml_single_t *tst, bool side);
static int upgradeToAsymmetric(eml_single_t *tst);
static int upgradeToStandardVaried(eml_single_t *tst);
static int upgradeToStandard(eml_single_t *tst);

static char *format_eml_number(eml_number *e);
static void print_standard_k(eml_standard_k *k);
static void print_standard_varied_k(eml_standard_varied_k *k);
static void print_single_t(eml_single_t *s);
static void print_super_t(eml_super_t *s);
static void print_emlobj(eml_obj *e);

static void free_single_t(eml_single_t *s);
static void free_super_t(eml_super_t *s);
static void free_emlobj(eml_obj *e);
static void free_results();
static void free_header();

static void initArray(Array *a, size_t size);
static void insertArray(Array *a, eml_obj obj);
static void freeArray(Array *a);

int main(int argc, char *argv[]){
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"squat\":5x5;"; // standard
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"squat\":5x(5,4,3,2,1);"; // standard varied
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"sl-rdl\":4x3:5x2;"; // asymetrical standard
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"sl-rdl\":4x(4,3,2,1):4x(4,3,2,1);"; // asymetrical standard varied
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"sl-rdl\"::4x(4,3,2,1);"; // asymetrical mixed
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"squat\":;"; // none
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"squat\"::;"; // asymetric none

    // Multiple
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"squat\":5x5;\"plyo-jump\":5x40;"; // standard multiple

    // With weight modifiers
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"squat\":5x5@120;"; // standard + weight
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"squat\":4x(4,3@30,2,1)@120;"; // standard varied inner modifier & macro
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"sl-rdl\":4x3@60:5xF@60;"; // asymetrical standard with modifiers
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"sl-rdl\":4x(4,3@30,2,1)@120:3x(F,F,F)@550;"; // asymetrical mixed

    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"squat\":5x60T@30;"; // standard + time + weight
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"squat\":4x(40T,30T@550,20T,10T)@120;"; // standard varied + time + weight
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"sl-rdl\":4x30T@440:5x30T@72;"; // asymetrical standard + time + weight
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"sl-rdl\":4x(40T@770,3@30,20T,1)@120:3x(F,FT,FT)@550;"; // asymetrical mixed

    // With RPE modifiers
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"squat\":5x5%100;"; // standard
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"squat\":5x(5,4%100,3,2@40000,1)@120;"; // standard varied with modifiers and macros
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"sl-rdl\":4x(40T@770,3%30,20T,1)@120:3x(F%100,FT%100,FT)%80;"; // asymetrical standard + time + weight + rpe

    // Fractional 
    // MAX: 21474835
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"squat\":8x7@.0;"; // standard fractional
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"sl-rdl\":4x(40T@770.99,3%30.50,20T,1)@120.2:3x(F%100,FT%100,FT)%80;"; // asymetrical standard + time + weight + rpe + frac

    // Superset/Circuit
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}super(\"squat\":5x5;\"squat\":4x4;);"; // standard
    char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}circuit(\"squat\":5x5;\"squat\":4x4;);"; // standard

    // Name 
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"nathans-super-epic-amazing-special-exercise-with-some-awesomely-cool-modifications-and-a-super-long-name-that-has-128-characters\":5x5;"; //max
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"E\":5x5;"; // min
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"abcdefghijklmnopqrstuvwxyz\":5x5;";

    int error = no_error;
    if ((error = parse(emlstring))) {
        #ifdef DEBUG
            printf("Failed with error: %d\n", error);
            printf("%s\n", emlstring);

            for(int i = 0; i < current_postition - 1; i++) {
                printf(" ");
            }

            printf("^\n");
        #endif
    }

    #ifdef DEBUG
        // Print & Free
        for(int i = 0; i < result.used; i++) {
            print_emlobj(&result.array[i]);
        }
        
        free_results();
        free_header();
    #endif

    return error;
}

/*
 * parse: Entry point for parsing eml. Starts at '{', ends at (emlstringlen - 1)
 */
int parse(char *eml_string) {
    emlString = eml_string;
    emlstringlen = strlen(eml_string);
    current_postition = 0;

    initArray(&result, 4);

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
            parse_header();

            #ifdef DEBUG
                printf("parsed version: %s, parsed weight: %s\n", version, weight);
                printf("-------------------------\n");
            #endif

            break;
        case (int)'s': // Give control to parse_super_t()
            if ((error = parse_super_t(&tsupt))) {
                goto bail;
            }

            eml_obj temp_super;
            temp_super.type = super;
            temp_super.data = tsupt;
            insertArray(&result, temp_super);

            break;
        case (int)'c': // Give control to parse_super_t()
            if ((error = parse_super_t(&tsupt))) {
                goto bail;
            }

            eml_obj temp_cirt;
            temp_cirt.type = circuit;
            temp_cirt.data = tsupt;
            insertArray(&result, temp_cirt);

            break;
        case (int)'\"': // Give control to parse_single_t() 
            if ((error = parse_single_t(&tst))) {
                goto bail;
            }

            eml_obj temp_single;
            temp_single.type = single;
            temp_single.data = tst;
            insertArray(&result, temp_single);

            break;
        case (int)';':
            ++current_postition;
            break;
        default:
            error = unexpected_error;
            
            if (tst != NULL) {
                free_single_t(tst);
            }

            if (tsupt != NULL) {
                free_super_t(tsupt);
            }

            goto bail;
        }
    }

    return no_error;

    bail:
        free_results();
        free_header();
        return error;
}

/*
 * parse_header: Parses header section or exits. Starts on "{" of header, ends on char succeeding "}"
*/
static int parse_header() {
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
            tht->next = header;
            header = tht;
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
    eml_single_t *tst = NULL;

    int error = no_error;
    Array dynamicSets; // janky, I don't like this

    initArray(&dynamicSets, 2);

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

                eml_obj temp;
                temp.type = single;
                temp.data = tst;
                insertArray(&dynamicSets, temp);

                break;
            case (int)')':
                // pass back tsupt
                *tsupt = calloc(1, sizeof(int) + sizeof(eml_single_t) * dynamicSets.used);
                if (*tsupt == NULL) {
                    for (int i = 0; i < dynamicSets.used; i++) {
                        free_emlobj(dynamicSets.array[i].data);
                    }
                    freeArray(&dynamicSets);
                    return allocation_error;
                }
                (*tsupt)->count = dynamicSets.used;
                for (int i = 0; i < dynamicSets.used; i++) {
                    (*tsupt)->sets[i] = ((eml_single_t*) dynamicSets.array[i].data);
                }
                freeArray(&dynamicSets);

                ++current_postition;
                return no_error;
            default: // skip {'u', 'p', 'e', 'r'} and {'i', 'r', 'c', 'u', 'i', 't'}
                ++current_postition;
                break;
        }
    }

    error = unexpected_error;

    if (dynamicSets.array != NULL) {
        for (int i = 0; i < dynamicSets.used; i++) {
            free_emlobj(dynamicSets.array[i].data);
        }
        freeArray(&dynamicSets);
    }

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
            if ((error = upgradeToAsymmetric(*tst))) {
                goto bail;
            }
            
            ++current_postition;
            break;
        case (int)'x':
            // Allocate standard_work & set sets.
            if ((error = upgradeToStandard(*tst))) {
                goto bail;
            }
            (*tst)->standard_work->sets = buffer_int;

            buffer_int = 0;
            kind = standard;

            ++current_postition;
            break;
        case (int)'(':
            // Allocate standard_varied_work & dealloc/transition standard_work
            if ((error = upgradeToStandardVaried(*tst))) {
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
                    (*tst)->standard_work->reps.toFailure = true;
                    break;
                case standard_varied:
                    // Apply 'toFailure' to internal Reps
                    if (vcount < (*tst)->standard_varied_work->sets) {
                        (*tst)->standard_varied_work->vReps[vcount].toFailure = true;
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
                    (*tst)->standard_work->reps.isTime = true;
                    break;
                case standard_varied:
                    // Apply 'isTime' to internal Reps
                    if (vcount < (*tst)->standard_varied_work->sets) {
                        (*tst)->standard_varied_work->vReps[vcount].isTime = true;
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
                moveToAsymmetric(*tst, 1);
            }

            ++current_postition;
            return no_error; // Give control back
        default:
            switch (dcount) {
                case 0: // Before radix
                    temp = buffer_int * 10U + (unsigned int)current - '0';

                    if (temp > 21474835U) { 
                        error = integral_overflow_error;
                        goto bail;
                    }

                    buffer_int = temp;
                    break;
                case 1: // 10ths place
                    temp = buffer_int + ((unsigned int)current - '0') * 10U;

                    if ((temp & eml_number_mask) > 2147483500U) {
                        error = fp_overflow_error;
                        goto bail;
                    }

                    buffer_int = temp;
                    dcount++;
                    break;
                case 2: // 100ths place
                    temp = buffer_int + ((unsigned int)current - '0');

                    if ((temp & eml_number_mask) > 2147483500U) {
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
        free_single_t(*tst);
        return error;
}

static void validate_header_t(eml_header_t *h) {
    if (strcmp(h->parameter, "version") == 0) {
        strncpy(version, h->value, 12);
        version[11] = '\0';
    }

    if (strcmp(h->parameter, "weight") == 0) {
        strncpy(weight, h->value, 4);
        weight[3] = '\0';
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
            
                strncpy(*result, strbuf, MAX_NAME_LENGTH + 1);
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
 * flush: Writes buf to the appropriate field in eml_single_t. If there is none work, flush will malloc tst->no_work. 
 *        If `vcount` is NULL, modifier will be applied as a macro. Resets `buf` and `dcount`.
 */
static int flush(eml_single_t *tst, uint32_t *vcount, eml_kind_flag kind, eml_modifier_flag mod, eml_number *buf, uint32_t *dcount) {
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
                    tst->standard_work->reps.mod = weight_mod;
                    tst->standard_work->reps.modifier.weight = *buf; 
                    break;
                case rpe_mod:
                    tst->standard_work->reps.mod = rpe_mod;
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
                            if (tst->standard_varied_work->vReps[i].mod == no_mod) {
                                tst->standard_varied_work->vReps[i].mod = weight_mod;
                                tst->standard_varied_work->vReps[i].modifier.weight = *buf;
                            }
                        }
                        break;
                    case rpe_mod:
                        for (int i = 0; i < tst->standard_varied_work->sets; i++) {
                            if (tst->standard_varied_work->vReps[i].mod == no_mod) {
                                tst->standard_varied_work->vReps[i].mod = rpe_mod;
                                tst->standard_varied_work->vReps[i].modifier.rpe = *buf;
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
                        tst->standard_varied_work->vReps[*vcount].mod = weight_mod;
                        tst->standard_varied_work->vReps[*vcount].modifier.weight = *buf;
                        break;
                    case rpe_mod:
                        tst->standard_varied_work->vReps[*vcount].mod = rpe_mod;
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
 * moveToAsymmetric: Moves tst->(no_work | standard_work | standard_varied_work) kind to the left (false) or right (true) side of tst->asymmetric_work.
 */
static void moveToAsymmetric(eml_single_t *tst, bool side) {
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
 * upgradeToAsymmetric: Allocates tst->asymmetric_work & moves existing tst->(no_work | standard_work | standard_varied_work) kind to left side.
 */
static int upgradeToAsymmetric(eml_single_t *tst) {
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

    moveToAsymmetric(tst, 0);
    return no_error;
}

/*
 * upgradeToStandardVaried: Allocates tst->standard_varied_work & migrates tst->standard_work.
 */
static int upgradeToStandardVaried(eml_single_t *tst) {
    tst->standard_varied_work = malloc(sizeof(eml_reps) * tst->standard_work->sets + sizeof(eml_number));
    if (tst->standard_varied_work == NULL) {
        return allocation_error;
    }
    
    tst->standard_varied_work->sets = tst->standard_work->sets;

    // standard_varied_kind defaults
    for (int i = 0; i < tst->standard_varied_work->sets; i++) {
        tst->standard_varied_work->vReps[i].isTime = false;
        tst->standard_varied_work->vReps[i].toFailure = false;
        tst->standard_varied_work->vReps[i].mod = no_mod;
    }

    free(tst->standard_work);
    tst->standard_work = NULL;
    return no_error;
}

/*
 * upgradeToStandard: Allocates tst->standard_work.
 */
static int upgradeToStandard(eml_single_t *tst) {
    tst->standard_work = malloc(sizeof(eml_standard_k));
    if (tst->standard_work == NULL) {
        return allocation_error;
    }

    tst->standard_work->reps.isTime = false;
    tst->standard_work->reps.toFailure = false;
    tst->standard_work->reps.mod = no_mod;
    return no_error;
}

/*
 * format_eml_number: Returns a temporary formatted eml_number string which is valid until next call or exits.
 */
static char *format_eml_number(eml_number *e) {
    static char f[12];

    if (*e & eml_number_H) {
        uint32_t masked = (*e & eml_number_mask);

        sprintf(f, "%u.%u", masked / 100U, masked % 100U);
    } else {
        sprintf(f, "%u", *e);
    }

    if (f[11] != '\0') { // TODO: Get rid of this / pass the error through. This function is currently only used in DEBUG mode but if it were not...
        free_results();
        free_header();
        exit(1);
    }

    return f;
}

/*
 * print_standard_k: Prints an eml_standard_k to stdout.
 */
static void print_standard_k(eml_standard_k *k) {
    eml_reps reps = k->reps;

    if (reps.isTime) {
        printf("%i time sets ", k->sets);

        if (reps.toFailure) {
            printf("to failure ");
        } else {
            printf("of %s seconds ", format_eml_number(&reps.value));
        }
    } else {
        printf("%i sets ", k->sets);

        if (reps.toFailure) {
            printf("to failure ");
        } else {
            printf("of %s reps ", format_eml_number(&reps.value));
        }
    }

    switch (reps.mod) {
        case no_mod:
            break;
        case weight_mod:
            printf("with %s %s", format_eml_number(&reps.modifier.weight), weight);
            break;
        case rpe_mod:
            printf("with RPE of %s", format_eml_number(&reps.modifier.rpe));
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
    printf("--- Printing single_t ---\n");
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
    printf("-------------------- Super --------------------\n");
    for (int i = 0; i < s->count; i++) {
        print_single_t(s->sets[i]);
    }
    printf("------------------ Super End ------------------\n");
    return;
}

/*
 * print_circuit_t: Prints a eml_circuit_t to stdout.
 */
static void print_circuit_t(eml_circuit_t *c) {
    printf("------------------- Circuit -------------------\n");
    for (int i = 0; i < c->count; i++) {
        print_single_t(c->sets[i]);
    }
    printf("----------------- Circuit End -----------------\n");
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
    for(int i = 0; i < s->count; i++) {
        free_single_t(s->sets[i]);
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
 * free_results: Frees all eml_obj in 'result' Array.
 */
static void free_results() {
    for(int i = 0; i < result.used; i++) {
        free_emlobj(&result.array[i]);
    }
}

/*
 * free_header: Frees all eml_header_t in 'header' Linked List.
 */
static void free_header() {
    eml_header_t *h = header;
    while (header != NULL) {
        header = header->next;

        free(h->parameter);
        free(h->value);
        free(h);
        h = header;
    }
}

static void initArray(Array *a, size_t size) {
    a->array = calloc(size, sizeof(eml_obj));
    if (a->array == NULL) {
        exit(1);
    }
    a->used = 0;
    a->size = size;
}

static void insertArray(Array *a, eml_obj obj) {
    if (a->used == a->size) {
        a->size *= 2;
        a->array = realloc(a->array, a->size * sizeof(eml_obj));
        if (a->array == NULL) {
            free_results();
            exit(1);
        }
    }
    a->array[a->used++] = obj;
}

static void freeArray(Array *a) {
    free(a->array);
    a->array = NULL;
    a->used = a->size = 0;
}
