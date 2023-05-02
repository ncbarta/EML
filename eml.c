#include "eml.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define true 1
#define false 0

// Parser parameters
static char version[4]; // up to 9.9
static char weight[4];

static char *emlString;
static int emlstringlen;
static int current_postition;

static const struct HeaderToken empty_header_t;
static const struct Reps empty_reps;
static const struct Standard empty_standard_k;
static const struct StandardVaried empty_standard_varied_k;
static const struct Asymmetric empty_asymetric_k;
static const struct Single empty_single_t;
static const struct Superset empty_super_t;

static void validate_header_t(eml_header_t *h);
static void rolling_int(char new_char, eml_number *dest);

static void parse();
static void parse_header();
static void parse_header_t(eml_header_t* tht);
static eml_super_t *parse_super_t();
static eml_single_t *parse_single_t();

static void print_standard_k(eml_standard_k *k);
static void print_standard_varied_k(eml_standard_varied_k *k);
static void print_single_t(eml_single_t *s);
static void print_super_t(eml_super_t *s);
static void print_emlobj(eml_obj *e);

static void free_single_t(eml_single_t *s);
static void free_super_t(eml_super_t *s);
static void free_emlobj(eml_obj *e);
static void free_results();

typedef struct {
    eml_obj *array;
    size_t used;
    size_t size;
} Array;

static void initArray(Array *a, size_t size);
static void insertArray(Array *a, eml_obj obj);
static void freeArray(Array *a);

static Array result;

int main(int argc, char *argv[]){
    char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"squat\":5x5;"; // standard
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"squat\":5x(5,4,3,2,1);"; // standard varied
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"sl-rdl\":4x3:5x2;"; // asymetrical standard
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}\"sl-rdl\":4x(4,3,2,1):4x(4,3,2,1);"; // asymetrical standard
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

    // Superset/Circuit
    // char emlstring[] = "{\"version\":\"1.0\",\"weight\":\"lbs\"}super(\"squat\":5x5;\"squat\":4x4;);"; // standard
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
// the specified vReps[vcount], otherwise, it will apply `buf` as a macro.
static void flush(eml_single_t *tst, uint32_t *vcount, eml_kind_flag kind, eml_modifier_flag mod, eml_number *buf) {
    switch (kind) {
        case none:
            switch (mod) {
                case no_mod:
                    tst->no_work = malloc(sizeof(int));
                    cond_bail_parse_single_t(tst->no_work, tst);
                    break;
                default:
                    printf("You cannot add a modifier to none work");
                    free(tst);
                    free_results();
                    exit(1);
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
    return;
}

// Starts on NAME ('"'), ends on ';'
// Can not return NULL
static eml_single_t *parse_single_t() {
    eml_single_t *tst = calloc(1, sizeof(eml_single_t));
    if (tst == NULL) {
        free_results();
        exit(1);
    }

    eml_kind_flag kind = none;
    eml_modifier_flag modifier = no_mod; 
    bool body_single_t_nw = false; // Toggle between name & work
    uint32_t body_standard_varied_vcount = 0;  // Index of standard_varied_k.vReps[]
    bool is_asymetric = false;

    eml_number buffer_int = 0;

    while (current_postition < emlstringlen) {
        char current = emlString[current_postition];

        switch (current) {
        case (int)'\"':
            ++current_postition;
            break;
        case (int)':':
            // Upgrade to asymetric_k
            if (body_single_t_nw == true) {
                flush(tst, NULL, kind, modifier, &buffer_int);

                modifier = no_mod;
                is_asymetric = true;
                kind = none;

                // Upgrade to asymetric

                tst->asymmetric_work = malloc(sizeof(eml_asymmetric_k)); // Pull this out into it's own function
                cond_bail_parse_single_t(tst->asymmetric_work, tst);
                tst->asymmetric_work->left_none_k = NULL;
                tst->asymmetric_work->left_standard_k = NULL;
                tst->asymmetric_work->left_standard_varied_k = NULL;
                tst->asymmetric_work->right_none_k = NULL;
                tst->asymmetric_work->right_standard_k = NULL;
                tst->asymmetric_work->right_standard_varied_k = NULL;

                if (tst->no_work != NULL) {
                    tst->asymmetric_work->left_none_k = tst->no_work;
                    tst->no_work = NULL;
                }
                else if (tst->standard_work != NULL) {
                    tst->asymmetric_work->left_standard_k = tst->standard_work;
                    tst->standard_work = NULL;
                }
                else if (tst->standard_varied_work != NULL) {
                    tst->asymmetric_work->left_standard_varied_k = tst->standard_varied_work;
                    tst->standard_varied_work = NULL;
                }
            }

            body_single_t_nw = true;
            ++current_postition;
            break;
        case (int)'x':
            tst->standard_work = malloc(sizeof(eml_standard_k));
            cond_bail_parse_single_t(tst->standard_work, tst);
            tst->standard_work->sets = buffer_int; // UN-COND flush buffer_int
            tst->standard_work->reps.isTime = false;
            tst->standard_work->reps.toFailure = false;
            tst->standard_work->reps.mod = no_mod;
            buffer_int = 0;
            kind = standard;
            ++current_postition;
            break;
        case (int)'(':
            // Upgrade standard_kind to standard_varied_kind
            tst->standard_varied_work = malloc(sizeof(empty_reps) * tst->standard_work->sets + sizeof(eml_number)); // Pull this out to it's own function.
            cond_bail_parse_single_t(tst->standard_varied_work, tst);
            
            tst->standard_varied_work->sets = tst->standard_work->sets;

            // standard_varied_kind defaults
            for (int i = 0; i < tst->standard_varied_work->sets; i++) {
                tst->standard_varied_work->vReps[i].isTime = false;
                tst->standard_varied_work->vReps[i].toFailure = false;
                tst->standard_varied_work->vReps[i].mod = no_mod;
            }

            body_standard_varied_vcount = 0;

            free(tst->standard_work);
            tst->standard_work = NULL;

            kind = standard_varied;
            ++current_postition;
            break;
        case (int)',':
            if (body_standard_varied_vcount > tst->standard_varied_work->sets) {
                printf("Too many variable reps\n");
                exit(1);
            }

            flush(tst, &body_standard_varied_vcount, kind, modifier, &buffer_int);

            modifier = no_mod;
            body_standard_varied_vcount++;
            ++current_postition;
            break;
        case (int)')':
            if (body_standard_varied_vcount > tst->standard_varied_work->sets) {
                printf("Too many variable reps\n");
                exit(1);
            }

            flush(tst, &body_standard_varied_vcount, kind, modifier, &buffer_int);

            modifier = no_mod;
            body_standard_varied_vcount++;

            if (body_standard_varied_vcount < tst->standard_varied_work->sets) {
                printf("Too few variable reps\n");
                exit(1);
            }
            ++current_postition;
            break;
        case (int)'F':
            switch (kind) {
                case none:
                    printf("You cannot make no work to failure");
                    exit(1);
                case standard:
                    tst->standard_work->reps.toFailure = true;
                    break;
                case standard_varied:
                    if (body_standard_varied_vcount < tst->standard_varied_work->sets) {
                        tst->standard_varied_work->vReps[body_standard_varied_vcount].toFailure = true;
                    }
                    else {
                        printf("You cannot use 'to failure' as a macro");
                        exit(1);
                    }
                    break;
            }
            ++current_postition;
            break;
        case (int)'T':
            switch (kind) {
                case none:
                    printf("You cannot add a modifier to none work");
                    exit(1);
                case standard:
                    tst->standard_work->reps.isTime = true;
                    break;
                case standard_varied:
                    if (body_standard_varied_vcount < tst->standard_varied_work->sets) {
                        tst->standard_varied_work->vReps[body_standard_varied_vcount].isTime = true;
                    }
                    else {
                        printf("You cannot attach time as a macro");
                        exit(1);
                    }
                    break;
            }
            ++current_postition;
            break;
        case (int)'@':
            switch (kind) {
                case none:
                    printf("You cannot add a modifier to none work");
                    exit(1);
                case standard:
                    tst->standard_work->reps.value = buffer_int;
                    break;
                case standard_varied:
                    // check if attaching inner modifier (EX: 3x(5@120,...,...))
                    if (body_standard_varied_vcount < tst->standard_varied_work->sets) {
                        tst->standard_varied_work->vReps[body_standard_varied_vcount].value = buffer_int;
                    }
                    break;
            }

            modifier = weight_mod;
            buffer_int = 0;
            ++current_postition;
            break;
        case (int)'%':
            switch (kind) {
                case none:
                    printf("You cannot add a modifier to none work");
                    exit(1);
                case standard:
                    tst->standard_work->reps.value = buffer_int; // COND flush buffer_int
                    break;
                case standard_varied:
                    // check if attaching inner modifier (EX: 3x(5%120,...,...))
                    if (body_standard_varied_vcount < tst->standard_varied_work->sets) {
                        tst->standard_varied_work->vReps[body_standard_varied_vcount].value = buffer_int;
                    }
                    break;
            }

            modifier = rpe_mod;
            buffer_int = 0;
            ++current_postition;
            break;
        case (int)';':
            flush(tst, NULL, kind, modifier, &buffer_int);

            if (is_asymetric == true) {
                if (tst->no_work != NULL) {
                    tst->asymmetric_work->right_none_k = tst->no_work;
                    tst->no_work = NULL;
                }
                else if (tst->standard_work != NULL) {
                    tst->asymmetric_work->right_standard_k = tst->standard_work;
                    tst->standard_work = NULL;
                }
                else if (tst->standard_varied_work != NULL) {
                    tst->asymmetric_work->right_standard_varied_k = tst->standard_varied_work;
                    tst->standard_varied_work = NULL;
                }
            }

            return tst; // Give control back
        default:
            if (body_single_t_nw == false) {
                strncat(tst->name, &current, 1);
            }
            else {
                rolling_int(current, &buffer_int);
            }
            ++current_postition;
            break;
        }
    }

    // Throw error here for incomplete/incorrect eml string
    exit(1);

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

// Creates an integer character by character left to right
static void rolling_int(char new_char, eml_number *dest) {
    // TODO: Implement fixed-point
    *dest = (*dest) * 10 + (int)new_char - '0';
    return;
}

static void print_standard_k(eml_standard_k *k) {
    eml_reps reps = k->reps;

    if (reps.isTime) {
        printf("%i time sets ", k->sets);

        if (reps.toFailure) {
            printf("to failure ");
        } else {
            printf("of %i seconds ", reps.value);
        }
    } else {
        printf("%i sets ", k->sets);

        if (reps.toFailure) {
            printf("to failure ");
        } else {
            printf("of %i reps ", reps.value);
        }
    }

    switch (reps.mod) {
        case no_mod:
            break;
        case weight_mod:
            printf("with %i %s", reps.modifier.weight, weight);
            break;
        case rpe_mod:
            printf("with RPE of %i", reps.modifier.rpe);
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
        eml_standard_k shim = empty_standard_k;
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