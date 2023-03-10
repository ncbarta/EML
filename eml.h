// Utility
typedef int bool;

// Parser flag types
typedef enum WorkKindFlag { none, standard, standard_varied } kind_flag; // asymetric not included since it is technically 2. Also you wouldn't attach a modifier to asymetric, just it's components
typedef enum AttachingModifierFlag { no_mod, weight_mod, rpe_mod } modifier_flag; // is attaching a modifier to the current work

// Header Token
typedef struct HeaderToken {
    char parameter[24]; // maybe malloc these instead
    char value[24];
} header_t;

// Modifier
typedef struct Reps{
    int value;   // Count, or duration
    bool isTime; // is time rep
    int weight;  // default -1, mutually exclusive with rpe
    int rpe;     // default -1, mutually exclusive with weight
} reps;

// EML Work (kind)

typedef bool none_k;

typedef struct Standard {
    reps reps;
    int sets;
} standard_k;

typedef struct StandardVaried {
    int sets;
    reps vReps[];
} standard_varied_k;

typedef struct Asymetric {
    standard_k *left_standard_k;
    standard_varied_k *left_standard_varied_k;
    none_k *left_none_k;
    standard_k *right_standard_k;
    standard_varied_k *right_standard_varied_k;
    none_k *right_none_k;
} asymetric_k;

// EML Tokens

typedef struct Single {
    char name[128];
    none_k *no_work;
    standard_k *standard_work;
    standard_varied_k *standard_varied_work;
    asymetric_k *asymetric_work;
} single_t;

typedef struct Superset {
    int count;
    single_t *sets[];
} super_t;

typedef super_t circuit_t;

// EML Objects

typedef enum EMLObjtype { single, super, circuit } eml_objtype;
typedef struct EMLObj {
    eml_objtype type;
    void        *data;
} eml_obj;

// NOTES/TODO:
// apparently "_t" is reserved for POSIX - change later.
// Descriptive error handling
// - EX: printf("Too many variable reps\n"); -> You have 6 variable reps [...], but only designate 5 sets.
// Edge case '))' to close varied set & super at the same time - might just remove this feature or patch it up during error handling. Whichever's easier.
// - Also doccument this bc it's not on the spec
// Make an interface. 
// Make a free() function. Check ALL code to make sure it is freeing things correctly under all scenerios.
// EML may be a bit of a misnomer. It has properties of a markup language. but it actually might fall into the category of meta-language better.
// Function organization esp Array functions.