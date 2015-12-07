#include <stdint.h>

// Client Update Message
typedef struct {
    uint32_t type; // must be equal to 1
    uint32_t client_id;
    uint32_t server_id;
    uint32_t timestamp;
    uint32_t update;
}Client_Update;


// View Change Message
typedef struct {
    uint32_t type; // must be equal to 2
    uint32_t server_id;
    uint32_t attempted;
}View_Change;


// VC Proof Message
typedef struct {
    uint32_t type; // must be equal to 3
    uint32_t server_id;
    uint32_t installed;
}VC_Proof;


// Prepare Message
typedef struct {
    uint32_t type; // must be equal to 4
    uint32_t server_id;
    uint32_t view;
    uint32_t local_aru;
}Prepare;

// Proposal Message
typedef struct {
    uint32_t type; // must be equal to 5
    uint32_t server_id;
    uint32_t view;
    uint32_t seq;
    Client_Update update;
}Proposal;


// Accept Message
typedef struct {
    uint32_t type; // must be equal to 6
    uint32_t server_id;
    uint32_t view;
    uint32_t seq;
}Accept;


// Globally Ordered Update
typedef struct {
    uint32_t type; // must be equal to 7
    uint32_t server_id;
    uint32_t seq;
    Client_Update update;
}Globally_Ordered_Update;


// Prepare OK Message
typedef struct {
    uint32_t type; // must be equal to 8
    uint32_t server_id;
    uint32_t view;
    // The following fields form the data_list
    // structure mentioned in the paper
    //
    // a list of proposals
    uint32_t total_proposals; // total number of proposals in this message
    uint32_t total_globally_ordered_updates; // total number of globally ordered updates
    Proposal* proposals;
    //
    // a list of globally ordered updates
    // in this message
    Globally_Ordered_Update* globally_ordered_updates;
}Prepare_OK;
