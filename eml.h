// Utility
typedef int bool;

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
    single_t sets[];
} super_t;

// NOTES/TODO:
// apparently "_t" is reserved for POSIX - change later.
// Remove "EZEML" from project
// This new RD system makes it easier to do versioning.
// prefix vs postfix operators
// Bring enums into this file
// Descriptive error handling
// - EX: printf("Too many variable reps\n"); -> You have 6 variable reps [...], but only designate 5 sets.
// Figure out what memory is not getting initialized as 0