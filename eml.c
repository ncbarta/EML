#include "eml.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_NAME_LENGTH 128

#define true 1
#define false 0

// Parser parameters
static char version[4]; // up to 9.9
static char weight[4];

static char *emlString;
static int emlstringlen;
static int current_postition;

static Array result;

static const struct HeaderToken empty_header_t;

static void validate_header_t(eml_header_t *h);

static void parse();
static void parse_header();
static void parse_header_t(eml_header_t* tht);
static char *parse_string();
static eml_super_t *parse_super_t();
static eml_single_t *parse_single_t();

static void cond_bail_parse_single_t(void *p, eml_single_t *tst);
static void flush(eml_single_t *tst, uint32_t *vcount, eml_kind_flag kind, eml_modifier_flag mod, eml_number *buf, uint32_t *dcount);
static void moveToAsymmetric(eml_single_t *tst, bool side);
static void upgradeToAsymmetric(eml_single_t *tst);
static void upgradeToStandardVaried(eml_single_t *tst);
static void upgradeToStandard(eml_single_t *tst);

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

    // Name 
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"nathans-super-epic-amazing-special-exercise-with-some-awesomely-cool-modifications-and-a-super-long-name-that-has-128-characters\":5x5;"; //max
    char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"E\":5x5;"; // min
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"abcdefghijklmnopqrstuvwxyz\":5x5;";

    emlstringlen = strlen(emlstring);

    printf("EML String: %s, length: %i\n", emlstring, emlstringlen);

    emlString = malloc(sizeof(char) * emlstringlen + 1);
    strcpy(emlString, emlstring);

    // https://linux.die.net/man/3/asprintf

    // printf(emlString);
    // fflush(stdout);

    initArray(&result, 4);
    parse();

    // Print & Free
    for(int i = 0; i < result.used; i++) {
        print_emlobj(&result.array[i]);
        free_emlobj(&result.array[i]);
    }
    
    free(emlString);
    return 0;
}

static void parse() {
    current_postition = 0;

    eml_super_t *tsupt = NULL;
    eml_circuit_t *tcirt = NULL;
    eml_single_t *tst = NULL;

    while (current_postition < emlstringlen) {
        char current = emlString[current_postition];

        switch (current) {
        case (int)'{': // Give control to parse_header()
            parse_header();
            printf("parsed version: %s, parsed weight: %s\n", version, weight);
            printf("-------------------------\n");
            break;
        case (int)'s': // Give control to parse_super_t()
            tsupt = parse_super_t();

            eml_obj temp_super;
            temp_super.type = super;
            temp_super.data = tsupt;
            insertArray(&result, temp_super);

            break;
        case (int)'c': // Give control to parse_super_t() *probably
            tcirt = parse_super_t();

            eml_obj temp_cirt;
            temp_cirt.type = circuit;
            temp_cirt.data = tcirt;
            insertArray(&result, temp_cirt);

            break;
        case (int)'\"': // Give control to parse_single_t()
            tst = parse_single_t(tst);

            eml_obj temp_single;
            temp_single.type = single;
            temp_single.data = tst;
            insertArray(&result, temp_single);

            break;
        case (int)';':
            ++current_postition;
            break;
        case (int)')':
            ++current_postition;
            break;
        default:
            break;
        }
    }
    return;
}

// Starts on "{" of header, ends on char succeeding "}"
static void parse_header() {
    eml_header_t tht = empty_header_t;

    while (current_postition < emlstringlen) {
        char current = emlString[current_postition];

        switch (current){
        case (int)'{':
            current_postition++;
            break;
        case (int)'}': // Release control & inc
            validate_header_t(&tht);
            ++current_postition;
            return;
        case (int)',':
            validate_header_t(&tht);
            ++current_postition;
            break;
        case (int)'\"':
            parse_header_t(&tht); // Pass control to new_parse_header_t()
            break;
        default:
            exit(-1);
            break;
        }
    }
}

static void parse_header_t(eml_header_t* tht) {
    bool header_header_t_pv = false; // Toggle between parameter & value

    while (current_postition < emlstringlen) {
        char current = emlString[current_postition];

        switch (current) {
            case (int)'}': // Release control
                return;
            case (int)',': // Release control
                return;
            case (int)':':
                header_header_t_pv = true;
                ++current_postition;
                break;
            case (int)'\"':
                ++current_postition;
                break;
            default:
                if (header_header_t_pv == false) {
                    strncat(tht->parameter, &current, 1);
                }
                else {
                    strncat(tht->value, &current, 1);
                }
                ++current_postition;
                break;
        }
    }
}

// Starts on 's' of super. Ends on ')'.
static eml_super_t *parse_super_t() {
    eml_super_t *tsupt = NULL; // Will be allocated just in time
    eml_single_t *tst = NULL;

    Array dynamicSets; // janky, I don't like this

    initArray(&dynamicSets, 2);

    while (current_postition < emlstringlen) {
        char current = emlString[current_postition];

        switch (current) {
            case (int)'(':
                ++current_postition;
                break;
            case (int)'\"':
                tst = parse_single_t();
                break;
            case (int)';':
                ++current_postition;

                eml_obj temp;
                temp.type = single;
                temp.data = tst;
                insertArray(&dynamicSets, temp);

                break;
            case (int)')':
                // Closing a superset & single_t.standard_varied_work
                // if (single) {
                //     insertArray(&dynamicSets, tst);
                //     tst = empty_single_t;
                // }

                // pass back tsupt
                tsupt = calloc(1, sizeof(int) + sizeof(eml_single_t) * dynamicSets.used);
                if (tsupt == NULL) {
                    for (int i = 0; i < dynamicSets.used; i++) {
                        free(dynamicSets.array[i].data);
                    }
                    freeArray(&dynamicSets);
                    free_results();
                    exit(1);
                }
                tsupt->count = dynamicSets.used;
                for (int i = 0; i < dynamicSets.used; i++) {
                    tsupt->sets[i] = ((eml_single_t*) dynamicSets.array[i].data);
                }
                freeArray(&dynamicSets);

                return tsupt;
            default:
                ++current_postition;
                break;
        }
    }

    // Add error message
    exit(1);

    return NULL; // Should not return NULL
}

// Starts on NAME ('"'), ends on ';'
// Can not return NULL
static eml_single_t *parse_single_t() {
    eml_single_t *tst = calloc(1, sizeof(eml_single_t));
    if (tst == NULL) {
        free_results();
        exit(1);
    }

    tst->name = parse_string();
    cond_bail_parse_single_t(tst->name, tst);

    eml_kind_flag kind = none;
    eml_modifier_flag modifier = no_mod; 

    eml_number buffer_int = 0;  // Rolling eml_number
    uint32_t dcount = 0;        // If >0, writing to (dcount * 10)'ths place, >2 error
    uint32_t vcount = 0;        // Index of standard_varied_k.vReps[]

    uint32_t temp;              // Used for building eml_number in `default`

    ++current_postition; // Skip ':'
    while (current_postition < emlstringlen) {
        char current = emlString[current_postition];

        switch (current) {
        case (int)'\"':
            ++current_postition;
            break;
        case (int)':':
            // Upgrade to asymetric_k
            // Write value/modifier. If kind == standard_varied_work, write as macro.
            flush(tst, NULL, kind, modifier, &buffer_int, &dcount);

            modifier = no_mod;
            kind = none;

            // Upgrade existing eml_single_t to asymmetric
            upgradeToAsymmetric(tst);
            
            ++current_postition;
            break;
        case (int)'x':
            // Allocate standard_work & set sets.
            upgradeToStandard(tst);
            tst->standard_work->sets = buffer_int;

            buffer_int = 0;
            kind = standard;

            ++current_postition;
            break;
        case (int)'(':
            // Allocate standard_varied_work & dealloc/transition standard_work
            upgradeToStandardVaried(tst);

            vcount = 0;
            kind = standard_varied;

            ++current_postition;
            break;
        case (int)',':
            if (vcount > tst->standard_varied_work->sets) {
                printf("Too many variable reps\n");
                cond_bail_parse_single_t(NULL, tst);
            }

            // Write reps/(internal)modifiers
            flush(tst, &vcount, kind, modifier, &buffer_int, &dcount);

            vcount++;
            modifier = no_mod;

            ++current_postition;
            break;
        case (int)')':
            if (vcount > tst->standard_varied_work->sets) {
                printf("Too many variable reps\n");
                cond_bail_parse_single_t(NULL, tst);
            }

            // Write reps/(internal)modifiers
            flush(tst, &vcount, kind, modifier, &buffer_int, &dcount);

            vcount++;
            modifier = no_mod;

            if (vcount < tst->standard_varied_work->sets) {
                printf("Too few variable reps\n");
                cond_bail_parse_single_t(NULL, tst);
            }
            ++current_postition;
            break;
        case (int)'F':
            switch (kind) {
                case none:
                    printf("You cannot make 'no work' to failure");
                    cond_bail_parse_single_t(NULL, tst);
                case standard:
                    tst->standard_work->reps.toFailure = true;
                    break;
                case standard_varied:
                    // Apply 'toFailure' to internal Reps
                    if (vcount < tst->standard_varied_work->sets) {
                        tst->standard_varied_work->vReps[vcount].toFailure = true;
                    }
                    else {
                        printf("You cannot use 'to failure' as a macro");
                        cond_bail_parse_single_t(NULL, tst);
                    }
                    break;
            }

            ++current_postition;
            break;
        case (int)'T':
            switch (kind) {
                case none:
                    printf("You cannot add a modifier to none work");
                    cond_bail_parse_single_t(NULL, tst);
                case standard:
                    tst->standard_work->reps.isTime = true;
                    break;
                case standard_varied:
                    // Apply 'isTime' to internal Reps
                    if (vcount < tst->standard_varied_work->sets) {
                        tst->standard_varied_work->vReps[vcount].isTime = true;
                    }
                    else {
                        printf("You cannot attach time as a macro");
                        cond_bail_parse_single_t(NULL, tst);
                    }
                    break;
            }

            ++current_postition;
            break;
        case (int)'@':
            switch (kind) {
                case none:
                    printf("You cannot add a modifier to none work");
                    cond_bail_parse_single_t(NULL, tst);
                case standard:
                    tst->standard_work->reps.value = buffer_int;
                    break;
                case standard_varied:
                    // Apply weight modifier to internal Reps (EX: 3x(5@120,...,...))
                    if (vcount < tst->standard_varied_work->sets) {
                        tst->standard_varied_work->vReps[vcount].value = buffer_int;
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
                    printf("You cannot add a modifier to none work");
                    cond_bail_parse_single_t(NULL, tst);
                case standard:
                    tst->standard_work->reps.value = buffer_int;
                    break;
                case standard_varied:
                    // Apply weight modifier to internal Reps (EX: 3x(5%120,...,...))
                    if (vcount < tst->standard_varied_work->sets) {
                        tst->standard_varied_work->vReps[vcount].value = buffer_int;
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
                printf("You cannot have fractional sets");
                cond_bail_parse_single_t(NULL, tst);
            }

            if (dcount) {
                printf("You cannot have multiple radix points");
                cond_bail_parse_single_t(NULL, tst);
            }

            if (modifier == no_mod) {
                printf("You cannot have fractional reps / timesets");
                cond_bail_parse_single_t(NULL, tst);
            }

            // Set H bit, potential overflow handled in `default`
            buffer_int = buffer_int * 100U | eml_number_H; 

            ++dcount;
            ++current_postition;
            break;
        case (int)';':
            // Write value/modifier. If kind == standard_varied_work, write as macro.
            flush(tst, NULL, kind, modifier, &buffer_int, &dcount);

            if (tst->asymmetric_work != NULL) {
                moveToAsymmetric(tst, 1);
            }

            return tst; // Give control back
        default:
            switch (dcount) {
                case 0: // Before radix
                    temp = buffer_int * 10U + (unsigned int)current - '0';

                    if (temp > 21474835U) { 
                        printf("Overflow");
                        cond_bail_parse_single_t(NULL, tst);
                    }

                    buffer_int = temp;
                    break;
                case 1: // 10ths place
                    temp = buffer_int + ((unsigned int)current - '0') * 10U;

                    if ((temp & eml_number_mask) > 2147483500U) {
                        printf("FP Overflow");
                        cond_bail_parse_single_t(NULL, tst);
                    }

                    buffer_int = temp;
                    dcount++;
                    break;
                case 2: // 100ths place
                    temp = buffer_int + ((unsigned int)current - '0');

                    if ((temp & eml_number_mask) > 2147483500U) {
                        printf("FP Overflow");
                        cond_bail_parse_single_t(NULL, tst);
                    }

                    buffer_int = temp;
                    dcount++;
                    break;
                default:
                    printf("Too many numbers right of the radix point");
                    cond_bail_parse_single_t(NULL, tst);
                    break;
            }
            
            ++current_postition;
            break;
        }
    }

    // Throw error here for incomplete/incorrect eml string
    cond_bail_parse_single_t(NULL, tst);

    return NULL; // should never execute under normal conditions
}

static void validate_header_t(eml_header_t *h) {
    if (strcmp(h->parameter, "version") == 0) {
        strcpy(version, h->value);
    }

    if (strcmp(h->parameter, "weight") == 0) {
        strcpy(weight, h->value);
    }

    *h = empty_header_t;
}

static char *parse_string() {
    char *result;

    char strbuf[MAX_NAME_LENGTH + 1];
    uint32_t strindex = 0;

    ++current_postition; // skip '"'
    while (current_postition < emlstringlen) {
        char current = emlString[current_postition];

        switch (current) {
            case (int)'\"':
                ++current_postition;

                if (strindex == 0) {
                    printf("Strings must be at least one character");
                    free_results();
                    exit(1);
                }

                result = malloc(strindex + 1);
                if (result == NULL) {
                    free_results();
                    exit(1);
                }
            
                strncpy(result, strbuf, MAX_NAME_LENGTH + 1);
                result[strindex] = '\0';
                return result;
            default:
                if (strindex > 127) {
                    printf("String must be 128 characters or less");
                    free_results();
                    exit(1);
                }

                strbuf[strindex++] = current;
                ++current_postition;
                break;
        }
    }

    return NULL;
}

// Frees up memory and stops execution of parse_single_t if *p is NULL
static void cond_bail_parse_single_t(void *p, eml_single_t *tst) {
    if (p == NULL) {
        free_single_t(tst);
        free_results();
        exit(1);
    }
}

// Writes buffer_int to the appropriate field, whether it be reps.value, or a modifier.
// if `vcount` is defined and there is a modifier to be applied, flush will write to 
// the specified vReps[vcount], otherwise, it will apply `buf` as a macro. If there is none work,
// flush will malloc tst->no_work. flush() does not write to sets.
static void flush(eml_single_t *tst, uint32_t *vcount, eml_kind_flag kind, eml_modifier_flag mod, eml_number *buf, uint32_t *dcount) {
    if (*dcount == 1) {
        printf("There must be at least one digit following the radix point.");
        cond_bail_parse_single_t(NULL, tst);
    }

    switch (kind) {
        case none:
            switch (mod) {
                case no_mod:
                    tst->no_work = malloc(sizeof(int));
                    cond_bail_parse_single_t(tst->no_work, tst);
                    break;
                default:
                    printf("You cannot add a modifier to none work");
                    cond_bail_parse_single_t(NULL, tst);
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
    return;
}

// Moves eml_single_t->(no_work | standard_work | standard_varied_work) to the left (false) or right (true) side of asymmetric_work
// Precondition: eml_single_t->asymmetric_work must be initialized
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

// Allocates asymmetric & moves existing eml_single_t->(no_work | standard_work | standard_varied_work) kind to left side.
static void upgradeToAsymmetric(eml_single_t *tst) {
    tst->asymmetric_work = malloc(sizeof(eml_asymmetric_k));
    cond_bail_parse_single_t(tst->asymmetric_work, tst);

    tst->asymmetric_work->left_none_k = NULL;
    tst->asymmetric_work->left_standard_k = NULL;
    tst->asymmetric_work->left_standard_varied_k = NULL;
    tst->asymmetric_work->right_none_k = NULL;
    tst->asymmetric_work->right_standard_k = NULL;
    tst->asymmetric_work->right_standard_varied_k = NULL;

    moveToAsymmetric(tst, 0);
}

// Allocates standard_varied_work & transitions existing eml_single_t->standard_work.
static void upgradeToStandardVaried(eml_single_t *tst) {
    tst->standard_varied_work = malloc(sizeof(eml_reps) * tst->standard_work->sets + sizeof(eml_number));
    cond_bail_parse_single_t(tst->standard_varied_work, tst);
    
    tst->standard_varied_work->sets = tst->standard_work->sets;

    // standard_varied_kind defaults
    for (int i = 0; i < tst->standard_varied_work->sets; i++) {
        tst->standard_varied_work->vReps[i].isTime = false;
        tst->standard_varied_work->vReps[i].toFailure = false;
        tst->standard_varied_work->vReps[i].mod = no_mod;
    }

    free(tst->standard_work);
    tst->standard_work = NULL;
}

// Allocates standard_work
static void upgradeToStandard(eml_single_t *tst) {
    tst->standard_work = malloc(sizeof(eml_standard_k));
    cond_bail_parse_single_t(tst->standard_work, tst);
    tst->standard_work->reps.isTime = false;
    tst->standard_work->reps.toFailure = false;
    tst->standard_work->reps.mod = no_mod;
    return;
}

// Returns a temporary formatted eml_number string which is valid until next call.
static char *format_eml_number(eml_number *e) {
    static char f[12];

    if (*e & eml_number_H) {
        uint32_t masked = (*e & eml_number_mask);

        sprintf(f, "%u.%u", masked / 100U, masked % 100U);
    } else {
        sprintf(f, "%u", *e);
    }

    if (f[11] != '\0') {
        free_results();
        exit(1);
    }

    return f;
}

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

static void print_super_t(eml_super_t *s) {
    printf("-------------------- Super --------------------\n");
    for (int i = 0; i < s->count; i++) {
        print_single_t(s->sets[i]);
    }
    printf("------------------ Super End ------------------\n");
    return;
}

static void print_emlobj(eml_obj *e) {
    if (e->type == single) {
        print_single_t((eml_single_t*) e->data);
    } else {
        print_super_t((eml_super_t*) e->data);
    }
}

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

static void free_super_t(eml_super_t *s) {
    for(int i = 0; i < s->count; i++) {
        free_single_t(s->sets[i]);
    }

    free(s);
}

static void free_emlobj(eml_obj *e) {
    if (e->type == single) {
        free_single_t((eml_single_t*) e->data);
    } else {
        free_super_t((eml_super_t*) e->data);
    }

    // free(e); not a pointer to dynamic memory
}

static void free_results() {
    for(int i = 0; i < result.used; i++) {
        free_emlobj(&result.array[i]);
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