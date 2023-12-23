#include <stdint.h>

typedef uint32_t bool;

/*
 * eml_number - An unsigned 32b fixed-point number
 *              MSB ("H") is reserved to "shift"
 *              decimal point by 2 places.
 *              
 *              Range: [0, 21,474,835.00]
 */
typedef uint32_t eml_number;
const uint32_t eml_number_mask = 0x7FFFFFFF;
const uint32_t eml_number_H = (0x1 << 31);

/*
 * Parser Flag Types:
 * eml_kind_flag - Which kind the parser is currently parsing excluding asymmetric since asymmetric is technically 2.
 * eml_modifier_flag - Which modifer is being attached to the current work.
 */
typedef enum WorkKindFlag { none, standard, standard_varied } eml_kind_flag;
typedef enum AttachingModifierFlag { no_mod, weight_mod, rpe_mod } eml_modifier_flag;

/*
 * eml_header_t - Header parameters used to define context.
 */
typedef struct HeaderToken {
    struct HeaderToken *next;
    char *parameter;
    char *value;
} eml_header_t;

/*
 * eml_reps - Number of reps (or number of seconds in a timeset) with optional modifiers.
 */
typedef struct Reps {
    eml_number value;

    union Modifier {
        eml_number weight;
        eml_number rpe;
    } modifier;

    // Decided against bitfields because of portability concerns
    uint8_t isTime;     // Reps defined by time (Ex: isometric exercises)
    uint8_t toFailure;  // Reps complete upon failure
    uint8_t mod;        // Which modifier (if any) is being applied to reps
                        // 0: No modifier
                        // 1: Weight
                        // 2: RPE
                        // 3-7: Reserved
} eml_reps;

/* EML Work (kind) */

/* 
 * No Work
 * Description: No work for designated exercise. Often used in asymmetric work to define unilateral exercises.
 * Example: "squat":;
 */
typedef bool eml_none_k;

/* 
 * Standard Work
 * Description: Sets & Reps.
 * Example: "squat":2x5; // two sets of five reps
 */
typedef struct Standard {
    eml_reps reps;
    uint32_t sets;
} eml_standard_k;

/* 
 * Standard Varied Work
 * Description: Sets & Varying Reps.
 * Example: "squat":2x(5,4); // two sets, one of five, then one of four
 */
typedef struct StandardVaried {
    uint32_t sets;
    eml_reps vReps[];
} eml_standard_varied_k;

/* 
 * Asymmetric
 * Description: Exercise separated by left/right side, each with their own kind of work (excluding asymmetric)
 * Example: "sl-rdl":5x5:4x4; // asymmetric single leg RDL's, five sets of five on left side, four sets of four on right.
 */
typedef struct Asymmetric {
    eml_standard_k        *left_standard_k;
    eml_standard_varied_k *left_standard_varied_k;
    eml_none_k            *left_none_k;
    eml_standard_k        *right_standard_k;
    eml_standard_varied_k *right_standard_varied_k;
    eml_none_k            *right_none_k;
} eml_asymmetric_k;

/* EML Tokens */

/*
 * eml_single_t - A single exercise with a `name` and work.
 */
typedef struct Single {
    char                  *name;
    eml_none_k            *no_work;
    eml_standard_k        *standard_work;
    eml_standard_varied_k *standard_varied_work;
    eml_asymmetric_k      *asymmetric_work;
} eml_single_t;

/*
 * eml_single_t - A linked list node holding onto eml_single_t within a super.
 */
typedef struct SuperMember {
    eml_single_t       *single;
    struct SuperMember *next;
} eml_super_member_t;

/*
 * eml_super_t - A collection of eml_super_member_t.
 */
typedef struct Superset {
    uint32_t           count;
    eml_super_member_t *sets;
} eml_super_t;

/*
 * eml_circuit_t - A collection of eml_super_member_t.
 */
typedef eml_super_t eml_circuit_t;

/* EML Objects */

/*
 * eml_objtype - The EML Token type.
 */
typedef enum EMLObjtype { single, super, circuit } eml_objtype;

/*
 * eml_obj - Wrapper around an EML Token.
 */
typedef struct EMLObj {
    eml_objtype   type;
    void          *data;
    struct EMLObj *next;
} eml_obj;

/*
 * eml_result - Parser output in the form of a linked list for the header and objects respectively
 */
typedef struct Result {
    eml_header_t *header;
    eml_obj      *objs;
} eml_result;

/*
 * Errors
 */
typedef enum Errors {
    no_error,                             // Program terminated successfully (or there's a REALLY bad error)
    unexpected_error,
    allocation_error,                     // Malloc returned an error
    name_work_separator_error,            // You must include the ':' separator between NAME and WORK
    extra_variable_reps_error,            // Too many variable reps
    missing_variable_reps_error,          // Too few variable reps
    none_work_to_failure_error,           // You cannot make 'no work' to failure
    to_failure_used_as_macro_error,       // You cannot use 'to failure' as a macro
    modifier_on_none_work_error,          // You cannot add a modifier to 'no work'
    time_macro_error,                     // You cannot attach time as a macro
    fractional_sets_error,                // You cannot have fractional sets
    multiple_radix_points_error,          // You cannot have multiple radix points
    fractional_none_modifier_value_error, // You cannot have fractional reps / timesets
    integral_overflow_error,              // eml_number has overflowed integral part.
    fp_overflow_error,                    // eml_number has overflowed while adding fp part.
    too_many_fp_digits,                   // Too many numbers right of the radix point
    empty_string_error,                   // Strings must be at least one character
    string_length_error,                  // String must be 128 characters or less
    missing_digit_following_radix_error,  // There must be at least one digit following the radix point
    missing_header_start_char,            // eml string must start with header '{'
    missing_version,                      // Header must contain version parameter
    missing_weight_unit,                  // Header must contain weight unit parameter
} eml_error;