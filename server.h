#include "message.h"
#include <map>
#include <sys/time.h>
#include <queue>
#include <string>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <vector>
#include <set>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <sys/select.h>
#include <list>


#define XINU 0
#define VM 1
#define paramDebug 0
#define errorDebug 0
#define funcDebug 0
#define CLIENT_UPDATE_MSG 1
#define VIEW_CHANGE_MSG 2
#define VC_PROOF_MSG 3
#define PREPARE_MSG 4
#define PROPOSAL_MSG 5
#define ACCEPT_MSG 6
#define GLOBAL_ORDERED_UPDATE_MSG 7
#define PREPARE_OK_MSG 8
#define PROGRESS_TIMER_MAX 32000000
#define VC_PROOF_MSG 3
#define MAXMSGSIZE 6000
#define CLIENT_REPLY "Executed"
#define CLIENT_REPLY_LEN 8
#define VCPROOF_TIMER_EXPIRY 4
using namespace std;

enum state{
    LEADER_ELECTION,
    REG_LEADER,
    REG_NONLEADER,
};

/* Server State variables */
typedef struct{
    int myServerID; // a unique identifier for this server
    state currState; // State of the server either of {LEADER ELECTION, REG LEADER, REG NONLEADER}
}stateVariables;

/* View State Variables */
typedef struct{
    int lastAttempted; // the last view this server attempted to install
    int lastInstalled; // the last view this server installed
    map<int, View_Change*> vcMessageMap; //Map of View Change messages, indexed by server id
    map<int, View_Change*>::iterator myIter;
}viewStateVariables;

/* Prepare Phase Variables */
typedef struct{
    Prepare* prepMsgLastView; // the Prepare message from last preinstalled view, if received
    map<int, Prepare_OK*> prepOkMessageMap; // Map of the prepare ok messages indexed by server id
    map<int, Prepare_OK*>::iterator myIter; // Map iterator
}PreparePhaseVariables;

/* Global History structure */

typedef struct{
    Proposal* latestProposalAcc; // latest Proposal accepted for this sequence number, if any
    map<int, Accept*> acceptMessageMap; // Map of corresponding Accept messages, indexed by server id
    map<int, Accept*>::iterator myIter; // Map iterator
    Globally_Ordered_Update* globalOrderUpdateMsg; // ordered update for this sequence number, if any
}GlobalHistory;

/* Global Ordering Variables */
typedef struct{
    int localAru; // the local aru value of this server
    int lastProposed;  // last sequence number proposed by the leader
    map<int, GlobalHistory*> globalHistorySeq; // map of global history indexed by seq number
    map<int, GlobalHistory*>::iterator myIter;
}GlobalOrderingVariables;

/* Timer Variables */
typedef struct{
    // Progress Timers - timeout on making global progress
    struct timeval progressTimers;
    time_t vcProofTimers;
    // Update Timers - timeout on globally ordering a specific update
    map<uint32_t, struct timeval> updateTimers;
    map<uint32_t, struct timeval>::iterator myIter;
}TimerVariables;

/* Client Handling Variables */
typedef struct{
    list<Client_Update*> clientUpdateMsgQueue; // queue of Client Update messages
    list< Client_Update* >::iterator myListIter; // queue of Client Update messages iterator
    map<int, int> lastExecuted; // Map of timestamps, indexed by client id
    map<int, int> lastEnqueued; // Map of timestamps, indexed by client id
    map<int, int>::iterator timeIter;
    map<int, Client_Update*> pendingUpdatesMsgMap; // Map of Client Update messages, indexed by client id
    map<int, Client_Update*>::iterator myIter; // Map of Client Update messages iterator
}clientHandlingVariables;

/* General System Variables */
typedef struct{
    long int paxosPortNum;
    long int serverPortNum;
    int totalNumProcess;

    // master FD list
    fd_set master; // master file descriptor list
    
    // TCP related variables
    //map<string, int> myTCPClientSocket; // map of all the active clients and IPADDRESS
    //map<uint32_t, string> myTCPClientID_IP; // map of all client id to IP
    map<uint32_t, uint32_t> myTCPClientID_FD;
    int myTCPListenSocket; // my listener TCP socket
    
    
    // UDP related variables
    int myUDPSendSocket;
    int myUDPRcvSocket;

    //  Timer flags
    bool isProgressTimerSet;
    bool isUpdateTimerSet;
    long progressTimerExpiry;
    long updateTimerExpiry;
    

    string hostFileName;
    string selfHostName;
    vector<string> hostNames;
    vector<string> hostIPAddr;
    map<string, string> hostNameToIP;
    map<string, uint32_t> ipToID;

}generalSystemInfo;

// Global Variables
generalSystemInfo mySystemInfo;
stateVariables myStateVariable; // state variable
viewStateVariables myViewState; // View State variables
TimerVariables myTimers; // timer variables
PreparePhaseVariables myPreparePhase; // Prepare phase variable
GlobalOrderingVariables myGlobalOrder; //  GlobalOrderingVariables
clientHandlingVariables myClientHandle; // clientHandlingVariables

// Function to reset the view state variables
void resetViewStateVariable();

// Get the address of the TCP Client
void *get_in_addr(struct sockaddr *sa);

// ShiftToLeaderElection Method
void shiftToLeaderElection(int view);

// Convert the byte order from host to network for messages
void convertHTONByteOrder(void* message, uint32_t msgType);

// Convert the byte order from network to host for messages
void convertNTOHByteOrder(void* message, uint32_t msgType);

// Construct the received message
void* constructRcvdMessage(int recvSocket, int type, int numProposal);

// Check if the received message is conflicting one or not
bool conflictMessage(void* message, int msgType);

// Function to update the data structures w.r.t received message
void updateDataStructures(void* message, int msgType);

// Function to handle the received messages
void handleMessage(void* message, int msgType);

// Preinstall ready
bool preInstallReady(int view);

// Check if leader of last attempted view
bool checkIfLeader(int lastAttempt);

// Function to construct the datalist for prepare ok message
void constructDataList(vector<Proposal*> &myPropList, vector<Globally_Ordered_Update*> &myGloupList, int aru);

// Function to construct the prepare ok message
Prepare_OK* constructPrepareOk(vector<Proposal*> myPropList, vector<Globally_Ordered_Update*> myGloupList, int view);

// Function to construct the prepare message
Prepare* constructPrepare(int view, int localAru);

// Function to construct the prepare message
Proposal* constructProposal(int view, int seqNum, Client_Update update);

// Function to construct the accept message
Accept* constructAccept(int view, int seqNum);

// Function to construct the globally ordered update message
Globally_Ordered_Update* constructGlobMsg(int seq);

// Function to construct the VC Proof message
VC_Proof* constructVCProofMsg();

// Funtion to check if the view is ready
bool viewPreparedReadyView(int view);

// Function to move to the prepare phase
void shiftToPreparePhase();

// Function to shift to REG lEADER state
void shiftToRegLeader();

// Function to shift to REG NON LEADER state
void shiftToRegNonLeader();

// Client Update handler
void clientUpdateHandler(Client_Update* updateMsg);

// Send proposal function
void sendProposal();

// enqueueUnboundPendingUpdates
void enqueueUnboundPendingUpdates();

// Check if a client update is bound or not.
bool isClientUpdateBound(Client_Update* updateMsg);

// Check if update is present in queue
bool isPresentInUpdateQueue(Client_Update* updateMsg);

// Enqueue Update
bool enqueueUpdate(Client_Update* updateMsg);

// Add the Client update to the pending update
void addToPendingUpdates(Client_Update* updateMsg);

// Function to remove the bound updates from the queue
void removeBoundUpdatesFromQueue();

bool checkLastEnqueuedTimeStamp(Client_Update* updateMsg);

bool checkLastExecutedTimeStamp(Client_Update* updateMsg);

// To check if this seq is globally oredered or not
bool globallyOrderedReady(int seq);

// Function to check if the accept map contains majority accepts from same view
bool checkForMajorityAcceptFromSameView(map<int, Accept*> acceptMap);

// Function to advance the All received upto of the leader
void advanceAru();

// Function to send messages to all the servers
void sendMsgToAllServers(void* message, int msgType);

// Function to send the message to leader
void sendToLeader(void* message, int msgType);

// Function to handle the functionality for executing the client update
void executeClientUpdate(Client_Update updateToExecute);

// Function to send the reply to a client after executing its update
void replyToClient(uint32_t clientID);

Prepare_OK* deserializePrepareOk(char* serialMsg);

char* serializePrepareOk(Prepare_OK* myMessage, int numProp, int numGlob);
