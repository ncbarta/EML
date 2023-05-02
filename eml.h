#include <stdint.h>

typedef unsigned int bool;

// EML Number is an unsigned 32b fixed-point number
// msb is reserved to "shift" decimal point by 2 places, meaning
// we can store numbers from [0, 21,474,835.00]
typedef uint32_t eml_number;

// Parser flag types
typedef enum WorkKindFlag { none, standard, standard_varied } eml_kind_flag; // asymetric not included since it is technically 2. Also you wouldn't attach a modifier to asymetric, just it's components
typedef enum AttachingModifierFlag { no_mod, weight_mod, rpe_mod } eml_modifier_flag; // is attaching a modifier to the current work

// Header Token
typedef struct HeaderToken {
    char parameter[24]; // maybe malloc these instead
    char value[24];
} eml_header_t;

// Modifier
typedef struct Reps {
    eml_number value;

    union Modifier {
        eml_number weight;
        eml_number rpe;
    } modifier;

    // Decided against bitfields because of portability concerns
    uint8_t isTime;     // Reps defined by time
    uint8_t toFailure;  // Reps complete upon failure
    uint8_t mod;        // Which modifier (if any) is being applied to reps
                        // 0: No modifier
                        // 1: Weight
                        // 2: RPE
                        // 3-7: Reserved
} eml_reps;

// EML Work (kind)

typedef bool eml_none_k;

typedef struct Standard {
    eml_reps reps;
    uint32_t sets;
} eml_standard_k;

typedef struct StandardVaried {
    uint32_t sets;
    eml_reps vReps[];
} eml_standard_varied_k;

typedef struct Asymmetric {
    eml_standard_k *left_standard_k;
    eml_standard_varied_k *left_standard_varied_k;
    eml_none_k *left_none_k;
    eml_standard_k *right_standard_k;
    eml_standard_varied_k *right_standard_varied_k;
    eml_none_k *right_none_k;
} eml_asymmetric_k;

// EML Tokens

typedef struct Single {
    char name[128];
    eml_none_k *no_work;
    eml_standard_k *standard_work;
    eml_standard_varied_k *standard_varied_work;
    eml_asymmetric_k *asymmetric_work;
} eml_single_t;

typedef struct Superset {
    uint32_t count;
    eml_single_t *sets[];
} eml_super_t;

typedef eml_super_t eml_circuit_t;

// EML Objects

typedef enum EMLObjtype { single, super, circuit } eml_objtype;
typedef struct EMLObj {
    eml_objtype type;
    void        *data;
} eml_obj;