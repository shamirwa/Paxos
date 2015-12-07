#include "server.h"
// 
// 1) How to keep track of the current view id for which prepare ok needs to be send
// coz current view will determine the current leader
// 
// 2) How to manage the update timers ?? Need to implement the update timer expiry
//
// 3) Correct the formation of prepare ok message and remember to send the length of proposal
// message in sendMsg functions.
//
// 4) Execute Client update changes and Proposal message changes
//
// Function to print the error messages
void error(const char msg[])
{
    if(errorDebug){
        fprintf(stderr,"Error: %s\n",msg);
        fflush(stderr);
    }
}

void error(const char msg[], int param){

    if(errorDebug){
        fprintf(stderr, "Error:%s %d\n", msg,param);
        fflush(stderr);
    }
}

void funcEntry(const char msg[]){
    if(funcDebug){
        fprintf(stderr, "Entered function %s", msg);
        fflush(stderr);
    }
}

// Function to print the usage of the application
void printUsage(){
    printf("Usage: server -h hostfile -p paxos_port -s server_port\n");
    exit(1);
}

// Function to validate the port number passed
bool verifyPortNumber(long portNumber){
    funcEntry("verifyPortNumber\n");

    if(portNumber < 1024 || portNumber >65535){
        return false;
    }
    return true;
}

// Function to validate the hostfile passed
bool verifyHostFileName(string fileName){
    funcEntry("verifyHostFileName\n");

    FILE *fp;
    fp = fopen(fileName.c_str(), "rb");
    if(!fp){
        return false;
    }

    int numLine = 0;
    char ch;

    do{
        ch = fgetc(fp);
        if(ch == '\n'){
            numLine++;
        }
    }while(ch != EOF);

    fclose(fp);

    mySystemInfo.totalNumProcess = numLine;

    return true;
}

// Function to reset the view state variable
void resetViewStateVariable(){

    funcEntry("resetViewStateVariable\n");
    myViewState.lastAttempted = 0;
    myViewState.lastInstalled = 0;
    myViewState.vcMessageMap.clear();
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
   funcEntry("get_in_Addr\n");
   if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// ShiftToLeaderElection Method
void shiftToLeaderElection(int view){

    funcEntry("shiftToLeaderElection\n");

    if(paramDebug){
        fprintf(stderr, "Attempting View: %d\n", view);
        fflush(stderr);
    }

    // Change my state to leader election
    myStateVariable.currState = LEADER_ELECTION;

    // Clear the Data Structures
    //
    // Free the memory from vc map
    for(myViewState.myIter = myViewState.vcMessageMap.begin();
        myViewState.myIter != myViewState.vcMessageMap.end();
        ++myViewState.myIter){

        if(myViewState.myIter->second){
            free(myViewState.myIter->second);
            myViewState.myIter->second = NULL;
        }
    }

    myViewState.vcMessageMap.clear();

    // Free the memory from prepok map
    for(myPreparePhase.myIter = myPreparePhase.prepOkMessageMap.begin();
        myPreparePhase.myIter != myPreparePhase.prepOkMessageMap.end();
        ++myPreparePhase.myIter){
        
        if(myPreparePhase.myIter->second){

            if(myPreparePhase.myIter->second->proposals){
                free(myPreparePhase.myIter->second->proposals);
                myPreparePhase.myIter->second->proposals = NULL;
            }

            if(myPreparePhase.myIter->second->globally_ordered_updates){
                myPreparePhase.myIter->second->globally_ordered_updates = NULL;
            }

            free(myPreparePhase.myIter->second);
            myPreparePhase.myIter->second = NULL;
        }
    }
    myPreparePhase.prepOkMessageMap.clear();

    if(!myPreparePhase.prepMsgLastView){
        free(myPreparePhase.prepMsgLastView);
        myPreparePhase.prepMsgLastView = NULL;
    }

    myClientHandle.lastEnqueued.clear();


    myViewState.lastAttempted = view;

    //Construct a View Change message
    View_Change* vcMessage = NULL;
    vcMessage = (View_Change*)malloc(sizeof(View_Change));
    memset(vcMessage, 0, sizeof(View_Change));

    vcMessage->type = VIEW_CHANGE_MSG;
    vcMessage->server_id = myStateVariable.myServerID;
    vcMessage->attempted = myViewState.lastAttempted;

    // Send the message to all servers
    sendMsgToAllServers(vcMessage, VIEW_CHANGE_MSG);

    //Convert the vcMessage back to host order before updating the data structures
    //DOING IN SEND MSG TO ALL SERVERS

    // SORABH APPLY VC TO DATA STRUCTURES
    updateDataStructures(vcMessage, VIEW_CHANGE_MSG);

    //sleep(3);
}

// Convert the byte order from network to host for message
void convertNTOHByteOrder(void* message, uint32_t msgType){

    funcEntry("convertNTOHByteOrder\n");

    if(errorDebug){
        fprintf(stderr, "MsgType: %d\n",msgType);
        fflush(stderr);
    }

    switch(msgType){
        case CLIENT_UPDATE_MSG:
            {
                Client_Update* newMessage = (Client_Update*)message;
                newMessage->type = ntohl(newMessage->type);
                newMessage->client_id = ntohl(newMessage->client_id);
                newMessage->server_id = ntohl(newMessage->server_id);
                newMessage->timestamp = ntohl(newMessage->timestamp);
                newMessage->update = ntohl(newMessage->update);

                break;
            }

        case VIEW_CHANGE_MSG:
            {
                View_Change* newMessage = (View_Change*)message;
                newMessage->type = ntohl(newMessage->type);
                newMessage->server_id = ntohl(newMessage->server_id);
                newMessage->attempted = ntohl(newMessage->attempted);
                break;
            }

        case VC_PROOF_MSG:
            {
                VC_Proof* newMessage = (VC_Proof*)message;
                newMessage->type = ntohl(newMessage->type);
                newMessage->server_id = ntohl(newMessage->server_id);
                newMessage->installed = ntohl(newMessage->installed);
                break;
            }
        case PREPARE_MSG:
            {
                Prepare* newMessage = (Prepare*)message;
                newMessage->type = ntohl(newMessage->type);
                newMessage->server_id = ntohl(newMessage->server_id);
                newMessage->view = ntohl(newMessage->view);
                newMessage->local_aru = ntohl(newMessage->local_aru);

                break;
            }
        case PROPOSAL_MSG:
            {
                Proposal* newMessage = (Proposal*)message;
                newMessage->type = ntohl(newMessage->type);
                newMessage->server_id = ntohl(newMessage->server_id);
                newMessage->view = ntohl(newMessage->view);
                newMessage->seq = ntohl(newMessage->seq);
                convertNTOHByteOrder(&(newMessage->update), CLIENT_UPDATE_MSG);

                break;
            }
        case ACCEPT_MSG:
            {
                Accept* newMessage = (Accept*)message;
                newMessage->type = ntohl(newMessage->type);
                newMessage->server_id = ntohl(newMessage->server_id);
                newMessage->view = ntohl(newMessage->view);
                newMessage->seq = ntohl(newMessage->seq);

                break;
            }
        case GLOBAL_ORDERED_UPDATE_MSG:
            {
                Globally_Ordered_Update* newMessage = (Globally_Ordered_Update*)message;
                newMessage->type = ntohl(newMessage->type);
                newMessage->server_id = ntohl(newMessage->server_id);
                newMessage->seq = ntohl(newMessage->seq);
                convertNTOHByteOrder(&(newMessage->update), CLIENT_UPDATE_MSG);
                break;
            }
        case PREPARE_OK_MSG:
            {
                Prepare_OK* newMessage = (Prepare_OK*)message;
                newMessage->type = ntohl(newMessage->type);
                newMessage->server_id = ntohl(newMessage->server_id);
                newMessage->view = ntohl(newMessage->view);
                newMessage->total_proposals = ntohl(newMessage->total_proposals);
                newMessage->total_globally_ordered_updates = ntohl(newMessage->total_globally_ordered_updates);

                for(int i=0; i<newMessage->total_proposals; i++){
                    convertNTOHByteOrder(&newMessage->proposals[i], PROPOSAL_MSG);
                }

                for(int i=0; i<newMessage->total_globally_ordered_updates; i++){
                    convertNTOHByteOrder(&newMessage->globally_ordered_updates[i], GLOBAL_ORDERED_UPDATE_MSG);
                }

                break;
            }
        default:
                error("Invalid Message type in convertNTOHByteOrder\n");

    }
}

// Convert the byte order from host to network for messages
void convertHTONByteOrder(void* message, uint32_t msgType){

    funcEntry("convertHTONByteOrder\n");

    if(paramDebug){
        fprintf(stderr, "Message Type:%d\n", msgType);
        fflush(stderr);
    }

    switch(msgType){
        case CLIENT_UPDATE_MSG:
            {
                Client_Update* newMessage = (Client_Update*)message;
                newMessage->type = htonl(newMessage->type);
                newMessage->client_id = htonl(newMessage->client_id);
                newMessage->server_id = htonl(newMessage->server_id);
                newMessage->timestamp = htonl(newMessage->timestamp);
                newMessage->update = htonl(newMessage->update);

                break;
            }

        case VIEW_CHANGE_MSG:
            {
                
                View_Change* newMessage = (View_Change*)message;
                newMessage->type = htonl(newMessage->type);
                newMessage->server_id = htonl(newMessage->server_id);
                newMessage->attempted = htonl(newMessage->attempted);
                break;
            }
        case VC_PROOF_MSG:
            {
                VC_Proof* newMessage = (VC_Proof*)message;
                newMessage->type = htonl(newMessage->type);
                newMessage->server_id = htonl(newMessage->server_id);
                newMessage->installed = htonl(newMessage->installed);
                break;
            }
        case PREPARE_MSG:
            {
                Prepare* newMessage = (Prepare*)message;
                newMessage->type = htonl(newMessage->type);
                newMessage->server_id = htonl(newMessage->server_id);
                newMessage->view = htonl(newMessage->view);
                newMessage->local_aru = htonl(newMessage->local_aru);

                break;
            }
        case PROPOSAL_MSG:
            {
                Proposal* newMessage = (Proposal*)message;
                newMessage->type = htonl(newMessage->type);
                newMessage->server_id = htonl(newMessage->server_id);
                newMessage->view = htonl(newMessage->view);
                newMessage->seq = htonl(newMessage->seq);
                convertHTONByteOrder(&(newMessage->update), CLIENT_UPDATE_MSG);

                break;
            }
        case ACCEPT_MSG:
            {
                Accept* newMessage = (Accept*)message;
                newMessage->type = htonl(newMessage->type);
                newMessage->server_id = htonl(newMessage->server_id);
                newMessage->view = htonl(newMessage->view);
                newMessage->seq = htonl(newMessage->seq);

                break;
            }
        case GLOBAL_ORDERED_UPDATE_MSG:
            {
                Globally_Ordered_Update* newMessage = (Globally_Ordered_Update*)message;
                newMessage->type = htonl(newMessage->type);
                newMessage->server_id = htonl(newMessage->server_id);
                newMessage->seq = htonl(newMessage->seq);
                convertHTONByteOrder(&(newMessage->update), CLIENT_UPDATE_MSG);

                break;
            }
        case PREPARE_OK_MSG:
            {
                Prepare_OK* newMessage = (Prepare_OK*)message;
                newMessage->type = htonl(newMessage->type);
                newMessage->server_id = htonl(newMessage->server_id);
                newMessage->view = htonl(newMessage->view);
                
                
                for(int i=0; i<newMessage->total_proposals; i++){
                    convertHTONByteOrder(&newMessage->proposals[i], PROPOSAL_MSG);
                }

                for(int i=0; i<newMessage->total_globally_ordered_updates; i++){
                    convertHTONByteOrder(&newMessage->globally_ordered_updates[i], GLOBAL_ORDERED_UPDATE_MSG);
                }
                
                newMessage->total_proposals = htonl(newMessage->total_proposals);
                newMessage->total_globally_ordered_updates = htonl(newMessage->total_globally_ordered_updates);
                
                break;
            }
        
        default:
                error("Invalid Message type in convertHTONByteOrder\n");

    }
}

// Function to check if the message is conflicting one
bool conflictMessage(void* message, int msgType){

    funcEntry("conflictMessage \n");
    
    bool isConflict = false;
    if(!message){
        error("Null message pointer received. So ignore the message\n");
        isConflict = true;
        return isConflict;
    }

    switch(msgType){
        case CLIENT_UPDATE_MSG:
            {
                // This message can be sent by a server
                // We need to check if I am leader or not. Coz only leader will
                // be receiving the client update message via UDP sockets from other
                // server SORABH
                if(myStateVariable.myServerID != ((myViewState.lastInstalled % mySystemInfo.totalNumProcess) + 1)){
                    isConflict = true;
                }
                break;
            }

        case VIEW_CHANGE_MSG:
            {
                View_Change* newMessage = (View_Change*)message;
                if(newMessage->server_id == myStateVariable.myServerID){

                    error("Server ID in msg:", newMessage->server_id);
                    isConflict = true;
                }

                if(myStateVariable.currState != LEADER_ELECTION){
                   
                    error("Current State:", myStateVariable.currState);
                    isConflict = true;
                }

                if(mySystemInfo.isProgressTimerSet){
                    error("In prog timer cond\n");
                    isConflict = true;
                }

                if(newMessage->attempted <= myViewState.lastInstalled){

                    error("Attempted in msg:", newMessage->attempted);
                    isConflict = true;
                }
                break;
            }

        case VC_PROOF_MSG: 
            {
                VC_Proof* newMessage = (VC_Proof*)message;

                if(newMessage->server_id == myStateVariable.myServerID){
                    isConflict = true;
                }

                if(myStateVariable.currState != LEADER_ELECTION){
                    isConflict = true;
                }
                break;
            }
        case PREPARE_MSG:
            {
                Prepare* newMessage = (Prepare*)message;

                if(newMessage->server_id == myStateVariable.myServerID){
                    error("Server id in msg:", newMessage->server_id);
                    isConflict = true;
                }

                if(newMessage->view != myViewState.lastAttempted){
                    error("view in msg:", newMessage->view);
                    isConflict = true;
                }
                break;
            }

        case PROPOSAL_MSG:
            {
                Proposal* newMessage = (Proposal*)message;

                if(newMessage->server_id == myStateVariable.myServerID){
                    error("Server ID in msg:", newMessage->server_id);
                    isConflict = true;
                }

                if(myStateVariable.currState != REG_NONLEADER){
                    error("My curr state:", myStateVariable.currState);
                    isConflict = true;
                }

                if(newMessage->view != myViewState.lastInstalled){
                    error("View in msg:", newMessage->view);
                    error("Last installed:", myViewState.lastInstalled);
                    isConflict = true;
                }
                break;
            }
        case ACCEPT_MSG:
            {
                Accept* newMessage = (Accept*)message;
                bool isGlobHistPresent = false;

                if(newMessage->server_id == myStateVariable.myServerID){
                    error("Server ID in msg:", newMessage->server_id);
                    isConflict = true;
                }

                if(newMessage->view != myViewState.lastInstalled){
                    error("View in message:", newMessage->view);
                    error("Last installed:", myViewState.lastInstalled);
                    isConflict = true;
                }

                // Need to check if a global history for this seq number exists
                // Then check if the proposal for this view exists in global history

                if((myGlobalOrder.globalHistorySeq.find(newMessage->seq) 
                                != myGlobalOrder.globalHistorySeq.end()) &&
                                (myGlobalOrder.globalHistorySeq[newMessage->seq])){
                    isGlobHistPresent = true;
                    
                    if(paramDebug){
                        fprintf(stderr, "Found the global history for this sequence: %d\n", newMessage->seq);
                        fflush(stderr);
                    }

                    Proposal* propMsg = myGlobalOrder.globalHistorySeq[newMessage->seq]->latestProposalAcc;

                    if(!propMsg){
                        error("Proposal Message not found for this accept\n");
                        isConflict = true;
                    }
                    else if(propMsg->view != newMessage->view){
                        
                        if(errorDebug){
                            fprintf(stderr, "View in stored msg %d and rcvd msg %d\n",propMsg->view, newMessage->view);
                            fflush(stderr);
                        }
                        isConflict = true;
                    }
                    break;
                }


                if(!isGlobHistPresent){
                    error("No global history for this sequence was found\n");
                    isConflict = true;
                }

                break;
            }
        case GLOBAL_ORDERED_UPDATE_MSG:
            {
                // this case is not there in the paper. check with others SORABH
                Globally_Ordered_Update* newMessage = (Globally_Ordered_Update*)message;

                if(newMessage->server_id == myStateVariable.myServerID){
                    error("Server ID in msg:", newMessage->server_id);
                    isConflict = true;
                }
                break;
            }
        case PREPARE_OK_MSG:
            {
                Prepare_OK* newMessage = (Prepare_OK*)message;

                if(myStateVariable.currState != LEADER_ELECTION){
                    error("My state:", myStateVariable.currState);
                    isConflict = true;
                }

                if(newMessage->view != myViewState.lastAttempted){
                    error("View in msg:", newMessage->view);
                    error("Last attempted:", myViewState.lastAttempted);
                    isConflict = true;
                }
                break;
            }
        default:
                error("Invalid Message type in conflictMessage\n");
    }

    return isConflict;
}

// Function to update the data structure when a message is received
void updateDataStructures(void* message, int msgType){

    funcEntry("updateDataStructures\n");

    switch(msgType){
        case CLIENT_UPDATE_MSG:
            break;

        case VIEW_CHANGE_MSG:
            {
                View_Change* newMessage = (View_Change*)message;
                bool isVcPresent = false;
                
                if(myViewState.vcMessageMap.find(newMessage->server_id)
                            != myViewState.vcMessageMap.end()){
                    
                    if(paramDebug){
                        fprintf(stderr, "ViewChange message exists for server: %d\n", newMessage->server_id);
                        fflush(stderr);
                    }


                    View_Change* vcStored = myViewState.vcMessageMap[newMessage->server_id];

                    if(!vcStored){
                        isVcPresent = false;
                        error("Somewhere update has not happened properly. We found entry in map but message is not there\n");
                    }
                    else{
                        isVcPresent = true;
                    }
                    break;
                }

                if(!isVcPresent){
                    myViewState.vcMessageMap[newMessage->server_id] = newMessage;
                }
                break;
            }

        case VC_PROOF_MSG:      break;
        case PREPARE_MSG:
            {
                Prepare* newMessage = (Prepare*)message;
                // Store the prepare message in prepare phase variable
                if(myPreparePhase.prepMsgLastView){
                    free(myPreparePhase.prepMsgLastView);
                    myPreparePhase.prepMsgLastView = NULL;
                }
                myPreparePhase.prepMsgLastView = newMessage;
                break;
            }
        case PROPOSAL_MSG:
            {
                Proposal* newMessage = (Proposal*)message;
               
                //Create Global History if not found for this seq
                if((myGlobalOrder.globalHistorySeq.find(newMessage->seq)
                            == myGlobalOrder.globalHistorySeq.end()) || 
                            (!myGlobalOrder.globalHistorySeq[newMessage->seq])){
                    
                    if(paramDebug){
                        fprintf(stderr, "No global history was found for seq number: %d\n", newMessage->seq);
                        fprintf(stderr,"Creating global history\n");
                        fflush(stderr);
                    }

                    myGlobalOrder.globalHistorySeq[newMessage->seq] = new GlobalHistory;
                    myGlobalOrder.globalHistorySeq[newMessage->seq]->latestProposalAcc = NULL;
                    myGlobalOrder.globalHistorySeq[newMessage->seq]->globalOrderUpdateMsg = NULL;
                    myGlobalOrder.globalHistorySeq[newMessage->seq]->acceptMessageMap.clear();
                    //memset(myGlobalOrder.globalHistorySeq[newMessage->seq], 0, sizeof(GlobalHistory));
                }

                // Found the Global History for this seq number
                Proposal* propMsg = myGlobalOrder.globalHistorySeq[newMessage->seq]->latestProposalAcc;
                Globally_Ordered_Update* globUpMsg = myGlobalOrder.globalHistorySeq[newMessage->seq]->globalOrderUpdateMsg;

                if(globUpMsg){
                    error("Found Glob Message so Return");
                    return;  // Not empty hence ignore the proposal
                }

                // If here then globUpMsg is empty
                if(propMsg){
                    error("Prop msg found\n");
                    if(newMessage->view > propMsg->view){
                        error("in second if\n");
                        // store the new proposal and clear the older accepts
                        //free(propMsg);
                        //propMsg = NULL;
                        myGlobalOrder.globalHistorySeq[newMessage->seq]->latestProposalAcc = newMessage;
                        myGlobalOrder.globalHistorySeq[newMessage->seq]->acceptMessageMap.clear();
                    }
                }
                else{
                    // No proposal message is there hence store it
                    myGlobalOrder.globalHistorySeq[newMessage->seq]->latestProposalAcc = newMessage;
                }

                break;
            }
        case ACCEPT_MSG:
            {
                Accept* newMessage = (Accept*)message;
                bool isAcceptPresent = false;

                if(errorDebug){
                    fprintf(stderr,"For accept message\n");
                    fflush(stderr);
                }

                if((myGlobalOrder.globalHistorySeq.find(newMessage->seq)
                        == myGlobalOrder.globalHistorySeq.end()) || 
                        (!myGlobalOrder.globalHistorySeq[newMessage->seq])){
                 
                    if(paramDebug){
                        fprintf(stderr, "No global history was found for seq number: %d\n", newMessage->seq);
                        fprintf(stderr, "Creating Global History\n");
                        fflush(stderr);
                    }

                    myGlobalOrder.globalHistorySeq[newMessage->seq] = new GlobalHistory;
                    myGlobalOrder.globalHistorySeq[newMessage->seq]->latestProposalAcc = NULL;
                    myGlobalOrder.globalHistorySeq[newMessage->seq]->globalOrderUpdateMsg = NULL;
                    myGlobalOrder.globalHistorySeq[newMessage->seq]->acceptMessageMap.clear();

                    //memset(myGlobalOrder.globalHistorySeq[newMessage->seq], 0, sizeof(GlobalHistory));
                }
                else{
                    if(errorDebug){
                        fprintf(stderr, "Global history found for this seq %d\n", newMessage->seq);
                        fflush(stderr);
                    }
                }

                // Found the global history
                Globally_Ordered_Update* globUpMsg = myGlobalOrder.globalHistorySeq[newMessage->seq]->globalOrderUpdateMsg;

                if(globUpMsg){
                    error("Returning 1\n");
                    return; // Not empty hence ignore this accept
                }

                if(myGlobalOrder.globalHistorySeq[newMessage->seq]->acceptMessageMap.size()
                        >= (mySystemInfo.totalNumProcess/2)){
                    error("Already contain N/2 accepts. Returning 2\n");
                    return; // Already contain floor(N/2) accept messages
                }

                // Check for the accept message
                if(myGlobalOrder.globalHistorySeq[newMessage->seq]->acceptMessageMap.find(newMessage->server_id)
                        != myGlobalOrder.globalHistorySeq[newMessage->seq]->acceptMessageMap.end()){
                    
                    if(paramDebug){
                        fprintf(stderr, "Found the accept message for sender server: %d\n", newMessage->server_id);
                        fflush(stderr);
                    }

                    Accept* accMsg = myGlobalOrder.globalHistorySeq[newMessage->seq]->acceptMessageMap[newMessage->server_id];

                    if(accMsg){
                        isAcceptPresent = true;
                        return; // Not empty do ignore the accept message;
                    }
                    else{
                        error("Accept not found. Some error in update\n");
                        isAcceptPresent = false;
                    }
                }

                if(!isAcceptPresent){
                    error("Accept not found so storing\n");
                    int seq = newMessage->seq;
                    int server = newMessage->server_id;

                    myGlobalOrder.globalHistorySeq[seq]->acceptMessageMap[server] = newMessage;
                }

                error("Leaving Update for accept with size: %d\n", myGlobalOrder.globalHistorySeq[newMessage->seq]->acceptMessageMap.size());

                break;
            }
        case GLOBAL_ORDERED_UPDATE_MSG: 
            {
                Globally_Ordered_Update* newMessage = (Globally_Ordered_Update*)message;

                if((myGlobalOrder.globalHistorySeq.find(newMessage->seq) 
                        == myGlobalOrder.globalHistorySeq.end()) ||
                        (!myGlobalOrder.globalHistorySeq[newMessage->seq])){

                    if(paramDebug){
                        fprintf(stderr, "No global history was found for seq number: %d\n", newMessage->seq);
                        fprintf(stderr, "Creating a new global history\n");
                        fflush(stderr);
                    }


                    myGlobalOrder.globalHistorySeq[newMessage->seq] = new GlobalHistory;
                    myGlobalOrder.globalHistorySeq[newMessage->seq]->latestProposalAcc = NULL;
                    myGlobalOrder.globalHistorySeq[newMessage->seq]->globalOrderUpdateMsg = NULL;
                    myGlobalOrder.globalHistorySeq[newMessage->seq]->acceptMessageMap.clear();
                    //memset(myGlobalOrder.globalHistorySeq[newMessage->seq], 0, sizeof(GlobalHistory));
                }

                Globally_Ordered_Update* globUpMsg = myGlobalOrder.globalHistorySeq[newMessage->seq]->globalOrderUpdateMsg;

                if(globUpMsg){
                    return; // Not empty hence ignore this accept
                }
                else{
                    myGlobalOrder.globalHistorySeq[newMessage->seq]->globalOrderUpdateMsg = newMessage;
                }

                break;
            }
        case PREPARE_OK_MSG:
            {
                Prepare_OK* newMessage = (Prepare_OK*)message;
                bool isPrepOkPresent = false;

                if(myPreparePhase.prepOkMessageMap.find(newMessage->server_id)
                        != myPreparePhase.prepOkMessageMap.end())
                {
                    if(paramDebug){
                        fprintf(stderr, "Prepare OK message for this server: %d exists\n", newMessage->server_id);
                        fflush(stderr);
                    }

                    Prepare_OK* prepOkStored = myPreparePhase.prepOkMessageMap[newMessage->server_id];

                    if(!prepOkStored){
                        error("For Prepare OK\n");
                        error("Somewhere update has not happened properly. We found entry in map but message is not there\n");
                        isPrepOkPresent = false;
                    }
                    else{
                        isPrepOkPresent = true;
                    }
                    break;
                }


                if(!isPrepOkPresent){
                    myPreparePhase.prepOkMessageMap[newMessage->server_id] = newMessage;

                    // Apply each entry of the data list
                    for(int i=0; i < newMessage->total_proposals; i++){
                        updateDataStructures(&(newMessage->proposals[i]), PROPOSAL_MSG);
                    }

                    for(int j=0; j < newMessage->total_globally_ordered_updates; j++){
                        updateDataStructures(&(newMessage->globally_ordered_updates[j]), GLOBAL_ORDERED_UPDATE_MSG);
                    }
                }

                break;
            }
        default:
            error("Invalid Message type in updateDataStructures\n");

    }

}

// handle the received messages
void handleMessage(void* message, int msgType){
    funcEntry("handleMessage\n");

    switch(msgType){

        case CLIENT_UPDATE_MSG:
            {

                Client_Update* newMessage = (Client_Update*)message;

                if(paramDebug){
                    fprintf(stderr, "Received Client Update Message from client:%d\n", newMessage->client_id);
                    fflush(stderr);
                }

                // Call the update handler
                clientUpdateHandler(newMessage);

                break;

            }
        case VIEW_CHANGE_MSG:
            {
                View_Change* newMessage = (View_Change*)message;

                if((newMessage->attempted > myViewState.lastAttempted) && !(mySystemInfo.isProgressTimerSet)){

                    if(paramDebug){
                        fprintf(stderr, "In 1st Cond for VC with view:%d from server%d\n",newMessage->attempted, newMessage->server_id);
                        fflush(stderr);
                    }

                    shiftToLeaderElection(newMessage->attempted);
                    updateDataStructures(newMessage, msgType);
                }

                if(newMessage->attempted == myViewState.lastAttempted){
                    
                    updateDataStructures(newMessage, msgType);

                    if(paramDebug){
                        fprintf(stderr, "In 2nd Cond for VC with view: %d from server%d\n", newMessage->attempted, newMessage->server_id);
                        fflush(stderr);
                    }


                    if(preInstallReady(newMessage->attempted)){

                        if(mySystemInfo.progressTimerExpiry < PROGRESS_TIMER_MAX){
                            mySystemInfo.progressTimerExpiry = mySystemInfo.progressTimerExpiry * 2;
                        }
                        gettimeofday(&myTimers.progressTimers, NULL);
                        mySystemInfo.isProgressTimerSet = true;

                        if(checkIfLeader(myViewState.lastAttempted)){
                            shiftToPreparePhase();
                        }
                    }
                }
                break;
            }

        case VC_PROOF_MSG:   
            {

                VC_Proof* newMessage = (VC_Proof*)message;

                if(paramDebug){
                    fprintf(stderr, "Received VC Proof message from server:%d\n", newMessage->server_id);
                    fflush(stderr);
                }

                if(newMessage->installed > myViewState.lastInstalled){
                    myViewState.lastAttempted = newMessage->installed;

                    if(checkIfLeader(myViewState.lastAttempted)){
                        shiftToPreparePhase();
                    }
                    else{
                        shiftToRegNonLeader();
                    }
                }

                // we are not storing VCPRoof message so can delete it
                if(newMessage){
                    free(message);
                    newMessage = NULL;
                    message = NULL;
                }
                break;
            }
        case PREPARE_MSG:
            {

                Prepare* newMessage = (Prepare*)message;

                if(myStateVariable.currState == LEADER_ELECTION){ //Install the view

                    if(paramDebug){
                        fprintf(stderr, "Received prepare from server:%d in LeaderElectin\n", newMessage->server_id);
                        fflush(stderr);
                    }

                    // Apply prepare to data structures
                    updateDataStructures(newMessage, msgType);

                    // DataList variables
                    vector<Proposal*> proposalList;
                    vector<Globally_Ordered_Update*> gloupList;

                    // Fill the data list
                    constructDataList(proposalList, gloupList, newMessage->local_aru);

                    // construct the prepare ok message
                    Prepare_OK* prepOkMsg = NULL;
                    prepOkMsg = constructPrepareOk(proposalList, gloupList, newMessage->view);

                    // Store the prepare ok message into the list with selfID
                    if(myPreparePhase.prepOkMessageMap[myStateVariable.myServerID])
                    {
                        error("Prepare message already present in the system. So free it first\n");

                        free(myPreparePhase.prepOkMessageMap[myStateVariable.myServerID]);
                    }
                    myPreparePhase.prepOkMessageMap[myStateVariable.myServerID] = prepOkMsg;

                    shiftToRegNonLeader();

                    // Send the message to leader. SORABH. How to know who is the leader ??
                    sendToLeader(prepOkMsg, PREPARE_OK_MSG);

                }
                else
                { // Already Installed the view

                    Prepare_OK* prepOkMsg = myPreparePhase.prepOkMessageMap[myStateVariable.myServerID];

                    if(!prepOkMsg){
                        error("Some error in update. Prepare ok message is not there\n");
                        break;
                    }

                    // send the message to the leader. SORABH How to know who is the leader ??
                    sendToLeader(prepOkMsg, PREPARE_OK_MSG);
                }
                break;
            }
        case PROPOSAL_MSG:
            {
                Proposal* newMessage = (Proposal*)message;

                if(paramDebug){
                    fprintf(stderr, "Received proposal from server:%d with update: %d\n", newMessage->server_id,
                                    newMessage->update.update);
                    fflush(stderr);
                }

                // Apply Proposal message to data structres
                updateDataStructures(newMessage, PROPOSAL_MSG);

                //contruct the accept message
                Accept* accMsg = constructAccept(newMessage->view, newMessage->seq);

                // Send message all the servers
                sendMsgToAllServers(accMsg, ACCEPT_MSG);

                break;
            }
        case ACCEPT_MSG:
            {
                Accept* newMessage = (Accept*)message;

                if(paramDebug){
                    fprintf(stderr, "Received accept from server:%d\n", newMessage->server_id);
                    fflush(stderr);
                }

                // Apply Accept to the data structures
                updateDataStructures(newMessage, ACCEPT_MSG);

                if(globallyOrderedReady(newMessage->seq))
                {
                    // Construct the globally ordered message
                    Globally_Ordered_Update* globMsg = constructGlobMsg(newMessage->seq);

                    // Apply to data structres
                    updateDataStructures(globMsg, GLOBAL_ORDERED_UPDATE_MSG);

                    advanceAru();

                }

                break;
            }
        case GLOBAL_ORDERED_UPDATE_MSG:
            {
                error("Something wrong happened. I am getting glob message\n");
                break;
            }
        case PREPARE_OK_MSG:
            {
                Prepare_OK* newMessage = (Prepare_OK*)message;

                if(paramDebug){
                    fprintf(stderr, "Received prepOk from server: %d\n", newMessage->server_id);
                    fflush(stderr);
                }

                // Apply the message to the data structures
                updateDataStructures(newMessage, msgType);

                if(viewPreparedReadyView(newMessage->view)){
                    shiftToRegLeader();
                }

                break;
            }
        default:
            error("Invalid Message type in handleMessage\n");

    }
}

// Check if the server is the leader of the passed view id
bool checkIfLeader(int lastAttempt){
    funcEntry("checkIfLeader\n");

    if(myStateVariable.myServerID == ((lastAttempt % mySystemInfo.totalNumProcess) + 1)){
        return true;
    }
    else{
        return false;
    }
}

// Function for preinstalling
bool preInstallReady(int view){
    funcEntry("preInstallReady\n");

    int majority = (mySystemInfo.totalNumProcess / 2) + 1; 

    if(myViewState.vcMessageMap.size() < majority){
        error("Less than floor(N/2) + 1 entries\n");
        return false;
    }

    int numEntryFound = 0;
    myViewState.myIter = myViewState.vcMessageMap.begin();

    for(; myViewState.myIter != myViewState.vcMessageMap.end(); myViewState.myIter++){
        
        if(myViewState.myIter->second->attempted == view){
            numEntryFound++;
        }
    }

    if(numEntryFound < majority){
        return false;
    }
    else{
        if(paramDebug){
            fprintf(stderr, "Majority VC message rcvd for view: %d by server: %d\n", view, myStateVariable.myServerID);
            fflush(stderr);
        }
        return true;
    }
}

// Function to construct the globally ordered update
Globally_Ordered_Update* constructGlobMsg(int seq)
{
    funcEntry("constructGlobMsg\n");

    Globally_Ordered_Update* message = NULL;
    message = (Globally_Ordered_Update*)malloc(sizeof(Globally_Ordered_Update));

    message->type = GLOBAL_ORDERED_UPDATE_MSG;
    message->server_id = myStateVariable.myServerID;
    message->seq = seq;

    // We are populating update as a random one as no where
    // we are using the update value in GLOB message
    // Insert the update from the latestProposal for this seq
    if(myGlobalOrder.globalHistorySeq.find(seq) 
            != myGlobalOrder.globalHistorySeq.end()){

        if(myGlobalOrder.globalHistorySeq[seq]->latestProposalAcc){
            
            memcpy(&(message->update), 
                    &(myGlobalOrder.globalHistorySeq[seq]->latestProposalAcc->update),
                    sizeof(Client_Update));
        }
        else{
            error("Latest proposal not found for this seq number\n");
            error("Not able to populate update in GLOB msg\n");
        }
    }
    else{
        error("Global history not found for this seq number\n");
        error("Not able to populate update in GLOB msg\n");
    }

    return message;
}

// Function to construct the datalist for prepare ok message
void constructDataList(vector<Proposal*> &myPropList, 
        vector<Globally_Ordered_Update*> &myGloupList, int aru){
    funcEntry("constructDataList\n");

    myGlobalOrder.myIter = myGlobalOrder.globalHistorySeq.begin();

    for(; myGlobalOrder.myIter != myGlobalOrder.globalHistorySeq.end();
          myGlobalOrder.myIter++){
        
        if((myGlobalOrder.myIter->first > aru) 
            && (myGlobalOrder.myIter->second))
        {
            error("Got the global history for seq number greater than aru for data list\n");
            
            if(myGlobalOrder.myIter->second->globalOrderUpdateMsg){
                error("There is globalOrderUpdateMessage \n");
                myGloupList.push_back(myGlobalOrder.myIter->second->globalOrderUpdateMsg);
            }
            else if(myGlobalOrder.myIter->second->latestProposalAcc){
                error("There is no globalOrderUpdateMessage So include Proposal\n");
                myPropList.push_back(myGlobalOrder.myIter->second->latestProposalAcc);
            }
            else{
                error("Some error in update. There is neither globalOrderUpdate Nor Proposal message\n");
            }
            
        }
    }

}

// Function to construct the accept message
Accept* constructAccept(int view, int seqNum)
{
    funcEntry("constructAccept\n");

    Accept* myMessage = NULL;
    myMessage = (Accept*) malloc(sizeof(Accept));

    myMessage->type = ACCEPT_MSG;
    myMessage->server_id = myStateVariable.myServerID;
    myMessage->view = view;
    myMessage->seq = seqNum;

    if(paramDebug){
        error("View:",myMessage->view);
        error("Seq:", myMessage->seq);
    }

    return myMessage;
}

// Function to construct the prepare ok message
//
// SORABH
//
Prepare_OK* constructPrepareOk(vector<Proposal*> myPropList, 
            vector<Globally_Ordered_Update*> myGloupList, int view)
{
    funcEntry("constructPrepareOk\n");

    int numProposalMsg = myPropList.size();
    int numGloupMsg = myGloupList.size();

    long propLen = sizeof(Proposal);
    long globLen = sizeof(Globally_Ordered_Update);

    if(errorDebug){
        fprintf(stderr, "Prop: %d Glob: %d\n", numProposalMsg, numGloupMsg);
        fflush(stderr);
    }

    Prepare_OK* prepOK = NULL;
    prepOK = (Prepare_OK*)malloc(sizeof(Prepare_OK));
    Proposal* propList = NULL;

    if(numProposalMsg > 0){
        propList = (Proposal*)malloc(propLen * numProposalMsg);
    }
    
    Globally_Ordered_Update* globList = NULL;

    if(numGloupMsg > 0){
        globList = (Globally_Ordered_Update*)malloc(globLen * numGloupMsg);
    }

    // Fill the prepare ok
    prepOK->type = PREPARE_OK_MSG;
    prepOK->server_id = myStateVariable.myServerID;
    prepOK->view = view;
    prepOK->total_proposals = numProposalMsg;
    prepOK->total_globally_ordered_updates = numGloupMsg;

    // copy the proposal message from the vector
    for(int i=0; i<numProposalMsg; i++){
        memcpy(&propList[i], myPropList[i], sizeof(Proposal));
    }

    // Copy all the gloup message from the vector
    for(int i=0; i<numGloupMsg; i++){
        memcpy(&globList[i], myGloupList[i], sizeof(Globally_Ordered_Update));
    }

    prepOK->proposals = propList;
    prepOK->globally_ordered_updates = globList;

    if(paramDebug){
        error("View:", prepOK->view);
        error("Total Prop:", prepOK->total_proposals );
        error("Total Glob:", prepOK->total_globally_ordered_updates);
    }

    return prepOK;

}

// Funtion to check if the view is ready
bool viewPreparedReadyView(int view)
{
    funcEntry("viewPreparedReadyView\n");

    int majority = (mySystemInfo.totalNumProcess / 2) + 1;

    if(myPreparePhase.prepOkMessageMap.size() < majority){
        error("Less than floor(N/2) + 1 entries\n");
        return false;
    }

    int numEntryFound = 0;
    myPreparePhase.myIter = myPreparePhase.prepOkMessageMap.begin();

    for(; myPreparePhase.myIter != myPreparePhase.prepOkMessageMap.end(); 
                myPreparePhase.myIter++){

        if(myPreparePhase.myIter->second->view == view){
            numEntryFound++;
        }
    }

    if(numEntryFound < majority){
        return false;
    }
    else{
        if(paramDebug){
            fprintf(stderr, "majority prepare ok from same view: %d is received\n", view);
            fflush(stderr);
        }
        return true;
    }

}

// Function to shift to prepare phase
void shiftToPreparePhase(){
    funcEntry("shiftToPreparePhase\n");
  
    //myStateVariable.currState = PREPARE_PHASE;
    myViewState.lastInstalled = myViewState.lastAttempted;

    // If progress timer is not set then set it here
    if(!mySystemInfo.isProgressTimerSet){

        if(mySystemInfo.progressTimerExpiry < PROGRESS_TIMER_MAX){
            mySystemInfo.progressTimerExpiry = mySystemInfo.progressTimerExpiry * 2;
        }
        gettimeofday(&myTimers.progressTimers, NULL);
        mySystemInfo.isProgressTimerSet = true;
    }

    // Construct the prepare message
    Prepare* prepMsg = constructPrepare(myViewState.lastInstalled, 
                                        myGlobalOrder.localAru);

    // Apply prepare message to the data structres
    updateDataStructures(prepMsg, PREPARE_MSG);

    // DataList variables
    vector<Proposal*> proposalList;
    vector<Globally_Ordered_Update*> gloupList;
    
    // Fill the data list
    constructDataList(proposalList, gloupList, myGlobalOrder.localAru);

    // Construct the prepare ok message
    Prepare_OK* prepOkMsg = constructPrepareOk(proposalList, gloupList, myViewState.lastInstalled);

    // store the prepare ok message
    if(myPreparePhase.prepOkMessageMap[myStateVariable.myServerID])
    {
        error("Prepare message already present in the system. So free it first\n");

        free(myPreparePhase.prepOkMessageMap[myStateVariable.myServerID]);
    }
    myPreparePhase.prepOkMessageMap[myStateVariable.myServerID] = prepOkMsg;

    myClientHandle.lastEnqueued.clear();

    // We will not write the information to the disk as not reqd.

    // Send the Prepare message to all the servers. SORABH
    sendMsgToAllServers(prepMsg, PREPARE_MSG);
}

// Function to construct the prepare message
Prepare* constructPrepare(int view, int localAru)
{
    funcEntry("constructPrepare\n");

    Prepare* message = NULL;
    message = (Prepare*)malloc(sizeof(Prepare));
    message->type = PREPARE_MSG;
    message->server_id = myStateVariable.myServerID;
    message->view = view;
    message->local_aru = localAru;

    return message;
}


// Function to shift to REG LEADER state
void shiftToRegLeader()
{
    funcEntry("shiftToRegLeader\n");

    printf("%d: Server %d is the new leader of view %d\n", myStateVariable.myServerID,
            ((myViewState.lastInstalled % mySystemInfo.totalNumProcess) + 1),
            myViewState.lastInstalled);
    fflush(stdout);

    myStateVariable.currState = REG_LEADER;

    // Unbound pending updates enqueue
    enqueueUnboundPendingUpdates();

    // Remove the bound updates from the queue
    removeBoundUpdatesFromQueue();

    advanceAru();

    myGlobalOrder.lastProposed = myGlobalOrder.localAru;

    sendProposal();
}


// Function to shift to REG NON LEADER state
void shiftToRegNonLeader()
{
    funcEntry("shiftToRegNonLeader\n");

    myStateVariable.currState = REG_NONLEADER;

    if(!mySystemInfo.isProgressTimerSet){
        
        if(mySystemInfo.progressTimerExpiry < PROGRESS_TIMER_MAX){
            mySystemInfo.progressTimerExpiry = mySystemInfo.progressTimerExpiry * 2;
        }
        gettimeofday(&myTimers.progressTimers, NULL);
        mySystemInfo.isProgressTimerSet = true;
    }

    myViewState.lastInstalled = myViewState.lastAttempted;

    printf("%d: Server %d is the new leader of view %d\n", myStateVariable.myServerID,
                ((myViewState.lastInstalled % mySystemInfo.totalNumProcess) + 1),
                myViewState.lastInstalled);
    fflush(stdout);
   
    if(!myClientHandle.clientUpdateMsgQueue.empty()){
        myClientHandle.clientUpdateMsgQueue.clear();
    }
}

// Function to handle the client update messages
void clientUpdateHandler(Client_Update* updateMsg)
{
    funcEntry("clientUpdateHandler\n");

    switch(myStateVariable.currState){

        case LEADER_ELECTION:
            {
                if(updateMsg->server_id != myStateVariable.myServerID){
                    return;
                }

                if(enqueueUpdate(updateMsg)){
                    addToPendingUpdates(updateMsg);
                }
                break;
            }
        case REG_NONLEADER:
            {
                if(updateMsg->server_id == myStateVariable.myServerID){
                    addToPendingUpdates(updateMsg);

                    // send the message to the leader SORABH
                    sendToLeader(updateMsg, CLIENT_UPDATE_MSG);
                }

                break;
            }
        case REG_LEADER:
            {
                if(enqueueUpdate(updateMsg)){
                    if(updateMsg->server_id == myStateVariable.myServerID){
                        addToPendingUpdates(updateMsg);
                    }

                    sendProposal();
                }
                break;

            }
        default:
            error("Invalid state while handling the update message\n");
    }
}

// Function to send the proposals
void sendProposal()
{
    funcEntry("sendProposal\n");

    int seq = myGlobalOrder.lastProposed + 1;
    Client_Update cliUpdate;
    bool updateFound = false;
    
    if((myGlobalOrder.globalHistorySeq.find(seq) != myGlobalOrder.globalHistorySeq.end()) &&
        (myGlobalOrder.globalHistorySeq[seq])){

        Globally_Ordered_Update* myGloupMsg = myGlobalOrder.globalHistorySeq[seq]->globalOrderUpdateMsg;
        Proposal* myPropMsg = myGlobalOrder.globalHistorySeq[seq]->latestProposalAcc;

        if(myGloupMsg){
            error("GlobalHistory[seq].Gloup is not empty\n");

            myGlobalOrder.lastProposed++;
            sendProposal();
            return;
        }

        if(myPropMsg){
            error("GlobalHistory[seq].Proposal is not empty\n");
            memcpy(&cliUpdate, &(myPropMsg->update), sizeof(Client_Update));
            updateFound = true;
        }
    }

    if(!updateFound){
        if(myClientHandle.clientUpdateMsgQueue.empty()){
            return;
        }
        else{
            Client_Update* clientUpdate = myClientHandle.clientUpdateMsgQueue.front();
            myClientHandle.clientUpdateMsgQueue.pop_front();
            memcpy(&cliUpdate, clientUpdate, sizeof(Client_Update));
        }
    }

    // Construct the proposal message
    Proposal* propMsg = constructProposal(myViewState.lastInstalled, seq, cliUpdate);

    updateDataStructures(propMsg, PROPOSAL_MSG);

    myGlobalOrder.lastProposed = seq;

    // Send the proposal to all the servers SORABH
    sendMsgToAllServers(propMsg, PROPOSAL_MSG);

}

// Function to construct the proposal message
Proposal* constructProposal(int view, int seqNum, Client_Update update){
    funcEntry("constructProposal\n");

    Proposal* message = NULL;
    message = (Proposal*)malloc(sizeof(Proposal));

    message->type = PROPOSAL_MSG;
    message->server_id = myStateVariable.myServerID;
    message->view = view;
    message->seq = seqNum;
    memcpy(&(message->update), &update, sizeof(Client_Update));
    
    return message;
}

// enqueueUnboundPendingUpdates
void enqueueUnboundPendingUpdates(){
    funcEntry("enqueueUnboundPendingUpdates\n");

    myClientHandle.myIter = myClientHandle.pendingUpdatesMsgMap.begin();

    for(;myClientHandle.myIter != myClientHandle.pendingUpdatesMsgMap.end();
         myClientHandle.myIter++){

        if(!isClientUpdateBound(myClientHandle.myIter->second) &&
            !isPresentInUpdateQueue(myClientHandle.myIter->second)){

            error("Client update is not bound and not in update queue\n");

            enqueueUpdate(myClientHandle.myIter->second);
        }
    }
}

// Function to check last executed timestamp
bool checkLastExecutedTimeStamp(Client_Update* updateMsg){
    funcEntry("checkLastEnqueuedTimeStamp\n");

    if(myClientHandle.lastExecuted.find(updateMsg->client_id)
            != myClientHandle.lastExecuted.end()){
        error("TimeStamp in last executed found for this client\n");

        if(updateMsg->timestamp <= myClientHandle.lastExecuted[updateMsg->client_id])
        {
            return false;
        }
    }

    return true;
}

// Function to check last enqueued timestamp
bool checkLastEnqueuedTimeStamp(Client_Update* updateMsg){
    funcEntry("checkLastEnqueuedTimeStamp\n");

    if(myClientHandle.lastEnqueued.find(updateMsg->client_id)
        != myClientHandle.lastEnqueued.end())
    {
        error("TimeStamp in last enqueued found for this client\n");

        if(updateMsg->timestamp <= myClientHandle.lastEnqueued[updateMsg->client_id])
        {
            return false;
        }
    }

    return true;
}


bool enqueueUpdate(Client_Update* updateMsg)
{
    funcEntry("enqueueUpdate\n");

    // Check in last executed
    if(!checkLastExecutedTimeStamp(updateMsg)){
        return false;
    }
    
    // Check in last enqueued 
    if(!checkLastEnqueuedTimeStamp(updateMsg)){
        return false;
    }

    // Adding to the update queue
    myClientHandle.clientUpdateMsgQueue.push_back(updateMsg);

    myClientHandle.lastEnqueued[updateMsg->client_id] = updateMsg->timestamp;

    return true;
}

// Function to add the client update in the pending update map
void addToPendingUpdates(Client_Update* updateMsg)
{
    funcEntry("addToPendingUpdates\n");
    
    if(myClientHandle.pendingUpdatesMsgMap[updateMsg->client_id]){

        free(myClientHandle.pendingUpdatesMsgMap[updateMsg->client_id]);
    }

    myClientHandle.pendingUpdatesMsgMap[updateMsg->client_id] = updateMsg;

    // set the update timer for this client update
    // NEED TO IMPLEMENT THE LOGIC OF UPDATE TIME SORABH
    gettimeofday(&myTimers.updateTimers[updateMsg->client_id], NULL);
}


// Function to remove the bound updates from the queue
void removeBoundUpdatesFromQueue(){
    funcEntry("removeBoundUpdatesFromQueue\n");

    for(myClientHandle.myListIter = myClientHandle.clientUpdateMsgQueue.begin();
        myClientHandle.myListIter != myClientHandle.clientUpdateMsgQueue.end();)
    {

        Client_Update* queueMsg = *(myClientHandle.myListIter);

        if(isClientUpdateBound(queueMsg) || 
            !checkLastExecutedTimeStamp(queueMsg) ||
            (!checkLastEnqueuedTimeStamp(queueMsg) && 
                (queueMsg->server_id != myStateVariable.myServerID))){


            // Remove *myClientHandle.myListIter from update queue

            if(checkLastEnqueuedTimeStamp(queueMsg)){
                myClientHandle.lastEnqueued[queueMsg->client_id] = 
                        queueMsg->timestamp;
            }

            // Now remove the element pointed by iterator
            // Coz after removing the element the message will be deleted and above update
            // in if is not possible. Also move iterator back after remove, as after deleting 
            // it will point to new element and again loop will increase the iterator by one
            myClientHandle.clientUpdateMsgQueue.erase(myClientHandle.myListIter++);
        }
        else{
            ++myClientHandle.myListIter;
        }
    }
}


// Check if update msg is there in update queue
bool isPresentInUpdateQueue(Client_Update* updateMsg)
{
    funcEntry("isPresentInUpdateQueue\n");
    
    myClientHandle.myListIter = myClientHandle.clientUpdateMsgQueue.begin();

    for(; myClientHandle.myListIter != myClientHandle.clientUpdateMsgQueue.end();
          myClientHandle.myListIter++){

        if(*(myClientHandle.myListIter) == updateMsg){
            return true;
        }
    }

    return false;

}

// Check if a client update is bound
bool isClientUpdateBound(Client_Update* updateMsg){

    funcEntry("isClientUpdateBound\n");

    // Check for the update in client update message in the 
    // globally ordered update message stored in global history.
    // If a match is found then return true, else false
    if(!updateMsg){
        return false;
    }

    myGlobalOrder.myIter = myGlobalOrder.globalHistorySeq.begin();

    for(;myGlobalOrder.myIter != myGlobalOrder.globalHistorySeq.end();
         myGlobalOrder.myIter++){

        if(myGlobalOrder.myIter->second->latestProposalAcc){

            if(myGlobalOrder.myIter->second->latestProposalAcc->update.update 
                        == updateMsg->update){
                printf("Client update is bound with seq num %d\n", myGlobalOrder.myIter->first);
                fflush(stdout);
                return true;
            }
        }
    }

    return false;
}

// Check if there are floor(N/2) accepts from same view.
bool checkForMajorityAcceptFromSameView(map<int, Accept*> acceptMap)
{
    funcEntry("checkForMajorityAcceptFromSameView\n");

    // Create a local map for storing count of Accept message from a view
    map<int, int> viewAcceptNum;
    map<int, int>::iterator iter2;

    map<int, Accept*>::iterator iter1 = acceptMap.begin();
    for(; iter1 != acceptMap.end(); iter1++){

        if(iter1->second){

            viewAcceptNum[iter1->second->view]++;
        }
    }

    int majority = mySystemInfo.totalNumProcess / 2;

    for(iter2 = viewAcceptNum.begin(); iter2 != viewAcceptNum.end();
        iter2++){

        if(iter2->second >= majority){
            return true;
        }
    }

    return false;
}


// Function to check the globally ordered ready
bool globallyOrderedReady(int seq)
{
    funcEntry("globallyOrderedReady\n");

    if((myGlobalOrder.globalHistorySeq.find(seq) != myGlobalOrder.globalHistorySeq.end())
        && (myGlobalOrder.globalHistorySeq[seq])){

        if((myGlobalOrder.globalHistorySeq[seq]->latestProposalAcc) &&
            checkForMajorityAcceptFromSameView(myGlobalOrder.globalHistorySeq[seq]->acceptMessageMap))
        {
            return true;
        }
    }

    return false;
}

// Function to send reply to client
void replyToClient(uint32_t clientID){
    funcEntry("replyToClient\n");
    error("Client ID:", clientID);
    string clientIP;

    if(mySystemInfo.myTCPClientID_FD.find(clientID)
            != mySystemInfo.myTCPClientID_FD.end()){
        if(paramDebug){
            fprintf(stderr,"Client %d found with fd %d \n", clientID, mySystemInfo.myTCPClientID_FD[clientID]);
            fflush(stderr);
        }

        // Send the reply
        string str("Hello");
        int numBytes = send(mySystemInfo.myTCPClientID_FD[clientID], str.c_str(), str.length(), 0);
        if(numBytes == -1){
            error("Error %d while sending reply to client\n", errno);
        }

        if(paramDebug){
            fprintf(stderr, "numBytes: %d\n", numBytes);
        }

        if(errorDebug){
            fprintf(stderr, "Client id: %d, and Socket %d\n", clientID, 
                    mySystemInfo.myTCPClientID_FD[clientID]);
        }
        // Remove the descriptor from masterFDS
        FD_CLR(mySystemInfo.myTCPClientID_FD[clientID], &mySystemInfo.master);

        // Close the client socket after reply
        close(mySystemInfo.myTCPClientID_FD[clientID]);

    }
    else{
        if(paramDebug){
            fprintf(stderr,"This client id %d is not in my map\n", clientID);
            fprintf(stderr,"Something is probably wrong\n");
            fflush(stderr);
        }
    }

}

// Function to do functionality of executing client update
void executeClientUpdate(Client_Update updateToExecute){
    funcEntry("executeClientUpdate\n");

    if(updateToExecute.server_id == myStateVariable.myServerID){
        error("Update was for me. Sending reply to client\n");
        // send reply to clieny
        replyToClient(updateToExecute.client_id);

        if((myClientHandle.pendingUpdatesMsgMap.find(updateToExecute.client_id)
                    != myClientHandle.pendingUpdatesMsgMap.end()) &&
                (myClientHandle.pendingUpdatesMsgMap[updateToExecute.client_id])){

            // Remove the update timer for this client update
            myTimers.updateTimers.erase(updateToExecute.client_id);

            // Remove update message from pending updates
            free(myClientHandle.pendingUpdatesMsgMap[updateToExecute.client_id]);
            myClientHandle.pendingUpdatesMsgMap.erase(updateToExecute.client_id);

        }
    }

    myClientHandle.lastExecuted[updateToExecute.client_id] = updateToExecute.timestamp;

    if(myStateVariable.currState != LEADER_ELECTION){
        gettimeofday(&myTimers.progressTimers, NULL);
        mySystemInfo.isProgressTimerSet = true;
    }

    if(myStateVariable.currState == REG_LEADER){
        sendProposal();
    }
}


// Function to advance the aru of the leader
void advanceAru()
{
    funcEntry("advanceAru\n");

    int myAru = myGlobalOrder.localAru + 1;

    while(1){
        
        if(myGlobalOrder.globalHistorySeq[myAru] &&
                (myGlobalOrder.globalHistorySeq[myAru])->globalOrderUpdateMsg){
            
            
            Client_Update cliUpdate;
            memcpy(&cliUpdate, &((myGlobalOrder.globalHistorySeq[myAru])->globalOrderUpdateMsg->update),
                        sizeof(Client_Update));
            // Execute the client update here.
            printf("%d: Executed update %d with seq %d and view %d\n",myStateVariable.myServerID, 
                        cliUpdate.update, myAru, myViewState.lastInstalled);
            fflush(stdout);

            executeClientUpdate(cliUpdate);

            myGlobalOrder.localAru++;
            myAru++;
        }
        else{
            return;
        }
    }
}

// Function to send messages to all the servers
void sendMsgToAllServers(void* message, int msgType){
    funcEntry("sendMsgToAllServers\n");

    // First convert the message from host to network order
    convertHTONByteOrder(message, msgType);

    long messageLen = 0;

    switch(msgType){
    
        case VIEW_CHANGE_MSG:
            {
                messageLen = sizeof(View_Change);
                break;
            }

        case VC_PROOF_MSG:
            {
                messageLen = sizeof(VC_Proof);
                break;
            }
        case PREPARE_MSG:
            {
                messageLen = sizeof(Prepare);
                break;
            }
        case PROPOSAL_MSG:
            {
                messageLen = sizeof(Proposal);
                break;
            }
        case ACCEPT_MSG:
            {
                messageLen = sizeof(Accept);
                break;
            }
        case GLOBAL_ORDERED_UPDATE_MSG:
            {
                messageLen = sizeof(Globally_Ordered_Update);
                break;
            }
        default:
                error("Invalid Message type received in sendToAll\n");

    }

    struct sockaddr_in receiverAddr;

    if(mySystemInfo.myUDPSendSocket == -1){
        // Need to open a socket
        if((mySystemInfo.myUDPSendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1){
            error("Error while opening socket to send the message\n");
            exit(1);
        }
    }

    memset((char*)&receiverAddr, 0, sizeof(receiverAddr));
    receiverAddr.sin_family = AF_INET;
    receiverAddr.sin_port = htons(mySystemInfo.paxosPortNum);

    // Send this message to all the servers
    for(int i=1; i <= mySystemInfo.totalNumProcess; i++){

        if(i != myStateVariable.myServerID){
            string receiverIP = mySystemInfo.hostIPAddr[i-1];

            if(inet_aton(receiverIP.c_str(), &receiverAddr.sin_addr) == 0){
                error("INET_ATON Failed\n");
            }


            if(sendto(mySystemInfo.myUDPSendSocket, message, messageLen, 0,
                        (struct sockaddr*) &receiverAddr, sizeof(receiverAddr)) == -1){
                if(paramDebug){
                    fprintf(stderr, "%d: Failed to send the message type %d to %s",
                            myStateVariable.myServerID, msgType, receiverIP.c_str());
                    fflush(stderr);
                }
            }
            else{
                if(paramDebug){
                    fprintf(stderr, "%d: successfully sent the message type %d to %s\n",
                            myStateVariable.myServerID, msgType, receiverIP.c_str());
                    fflush(stderr);
                }
            }
        }

    }

    // Convert the message back to host order. Might be stored somewhere
    convertNTOHByteOrder(message, msgType);

}

// Function to serialize the prepare ok message
char* serializePrepareOk(Prepare_OK* myMessage, int numProp, int numGlob){
    funcEntry("serializePrepareOk\n");

    char* serialMsg = NULL;
    
    serialMsg = (char*)malloc(sizeof(Prepare_OK) + numProp * sizeof(Proposal) + 
                            numGlob * sizeof(Globally_Ordered_Update));

    // Copy the messages in serailMsg
    memcpy(serialMsg, myMessage, sizeof(Prepare_OK));

    if(numProp > 0){
        memcpy(serialMsg + sizeof(Prepare_OK), myMessage->proposals, sizeof(Proposal) * numProp);
    }

    if(numGlob > 0){
        memcpy(serialMsg + sizeof(Prepare_OK) + sizeof(Proposal) * numProp, myMessage->globally_ordered_updates, 
                        sizeof(Globally_Ordered_Update) * numGlob);
    }

    return serialMsg;

}

// Function to de-serialize the prepare ok message
Prepare_OK* deserializePrepareOk(char* serialMsg){
    funcEntry("deserializePrepareOk\n");
    
    Prepare_OK* rcvdPrepMsg = NULL;
    Proposal* rcvdPropMsg = NULL;
    Globally_Ordered_Update* rcvdGlobMsg = NULL;

    
    rcvdPrepMsg = (Prepare_OK*)malloc(sizeof(Prepare_OK));
    memcpy(rcvdPrepMsg, serialMsg, sizeof(Prepare_OK));
    int numProp = ntohl(rcvdPrepMsg->total_proposals);
    int numGlob = ntohl(rcvdPrepMsg->total_globally_ordered_updates);

    if(numProp > 0){
        rcvdPropMsg = (Proposal*)malloc(sizeof(Proposal) * numProp);

        memcpy(rcvdPropMsg, serialMsg + sizeof(Prepare_OK), sizeof(Proposal) * numProp);
    }

    if(numGlob > 0){
        rcvdGlobMsg = (Globally_Ordered_Update*)malloc(sizeof(Globally_Ordered_Update) * numGlob);

        memcpy(rcvdGlobMsg, serialMsg + sizeof(Prepare_OK) + sizeof(Proposal) * numProp, sizeof(Globally_Ordered_Update) * numGlob);
    }

    rcvdPrepMsg->proposals = rcvdPropMsg;
    rcvdPrepMsg->globally_ordered_updates = rcvdGlobMsg;

    return rcvdPrepMsg;
}

// Function to send the message to leader
void sendToLeader(void* message, int msgType)
{
    funcEntry("sendToLeader\n");
    int currLeader = (myViewState.lastInstalled % mySystemInfo.totalNumProcess) + 1;
    int messageLen = 0;

    // For prepare ok message
    Prepare_OK* originalMsg = NULL;
    int numProposal = 0;
    int numGlobal = 0;

    switch(msgType){
    
        case CLIENT_UPDATE_MSG:
            {
                messageLen = sizeof(Client_Update);
                
                break;
            }
        case VIEW_CHANGE_MSG:
            {
                messageLen = sizeof(View_Change);
                break;
            }
        case VC_PROOF_MSG:
            {
                messageLen = sizeof(VC_Proof);
                break;
            }
        case PREPARE_MSG:
            {
                messageLen = sizeof(Prepare);
                break;
            }
        case PREPARE_OK_MSG:
            {
                numProposal = ((Prepare_OK*)message)->total_proposals;
                numGlobal = ((Prepare_OK*)message)->total_globally_ordered_updates;

                if(errorDebug){
                    fprintf(stderr, "Num Prop: %d, NumGlob: %d\n", numProposal, numGlobal);
                    fflush(stderr);
                }

                messageLen = sizeof(Prepare_OK);
                break;
            }
        case PROPOSAL_MSG:
            {
                messageLen = sizeof(Proposal);
                break;
            }
        case ACCEPT_MSG:
            {
                messageLen = sizeof(Accept);
                break;
            }
        case GLOBAL_ORDERED_UPDATE_MSG:
            {
                messageLen = sizeof(Globally_Ordered_Update);
                break;
            }
        default:
                break;


    }

    struct sockaddr_in receiverAddr;

    if(mySystemInfo.myUDPSendSocket == -1){
        // Need to open a socket
        if((mySystemInfo.myUDPSendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1){
            error("Error while opening socket to send the message\n");
            exit(1);
        }
    }
    
    memset((char*)&receiverAddr, 0, sizeof(receiverAddr));
    receiverAddr.sin_family = AF_INET;
    receiverAddr.sin_port = htons(mySystemInfo.paxosPortNum);
    
    // Send this message to leader
    string receiverIP = mySystemInfo.hostIPAddr[currLeader - 1];
        
    if(inet_aton(receiverIP.c_str(), &receiverAddr.sin_addr) == 0){
        error("INET_ATON Failed\n");
    }

    
    //Convert the message to network order
    convertHTONByteOrder(message, msgType);

    if(msgType == PREPARE_OK_MSG){
        originalMsg = (Prepare_OK*)message;

        message = serializePrepareOk(originalMsg, numProposal, numGlobal);
        messageLen = sizeof(Prepare_OK) + sizeof(Proposal) * numProposal + sizeof(Globally_Ordered_Update) * numGlobal;
    }

    if(sendto(mySystemInfo.myUDPSendSocket, message, messageLen, 0,
        (struct sockaddr*) &receiverAddr, sizeof(receiverAddr)) == -1){
        
        if(paramDebug){
            fprintf(stderr, "%d: Failed to send the message type %d to leader: %s",
                     myStateVariable.myServerID, msgType, receiverIP.c_str());
            fflush(stderr);
            }
    }
    else{
        if(paramDebug){
            fprintf(stderr, "%d: successfully sent the message type %d to %s\n",
                        myStateVariable.myServerID, msgType, receiverIP.c_str());
            fflush(stderr);
        }
    }

    // Check for prepare ok message
    if(msgType == PREPARE_OK_MSG){
        if(message){
            free(message);
            message = NULL;
        }
        message = originalMsg;
        originalMsg = NULL;
    }

    // Convert the message back to host order. Might be stored somewhere
    convertNTOHByteOrder(message, msgType);
}

// Function to send the VC proof message
VC_Proof* constructVCProofMsg(){
    funcEntry("constructVCProofMsg\n");

    VC_Proof* message = (VC_Proof*)malloc(sizeof(VC_Proof));

    message->type = VC_PROOF_MSG;
    message->server_id = myStateVariable.myServerID;
    message->installed = myViewState.lastInstalled;

    return message;
     
}


// Main function
int main(int argc, char* argv[]){
    int argCount = 1;

    myStateVariable.currState = LEADER_ELECTION;

    // Reset the view state variable
    resetViewStateVariable();

    // Set the update and progress timer bound
    mySystemInfo.progressTimerExpiry = 2000000;
    mySystemInfo.updateTimerExpiry = 1000000;

    char* maxMessage = NULL;
    maxMessage = (char*)malloc(MAXMSGSIZE);
    char* rcvdMessage = NULL;

    if(!maxMessage){
        error("In malloc for max message\n");
        exit(1);
    }

    time(&myTimers.vcProofTimers);

    // Implement the command line option parser
    if(argc < 7)
    {
        // Insufficient number of options entered
        error("Invalid command entered\n");
        printUsage();
    }

    do{
        // Get the hostfile name
        if(strcmp(argv[argCount], "-h") == 0){
            string fileName(argv[++argCount]);
            if(verifyHostFileName(fileName)){
                mySystemInfo.hostFileName = fileName;
            }
            else{
                error("Invalid hostfile name entered\n");
                exit(1);
            }
            argCount++;
        }

        // Get the paxos port number
        if(strcmp(argv[argCount], "-p") == 0){
            long paxosPort = atoi(argv[++argCount]);
            if(verifyPortNumber(paxosPort)){
                mySystemInfo.paxosPortNum =  paxosPort;
            }
            else{
                error("Invalid Paxos Port Number entered\n");
                exit(1);
            }
            argCount++;
        }

        // Get the server port number
        if(strcmp(argv[argCount], "-s") == 0){
            long serverPort = atoi(argv[++argCount]);
            if(verifyPortNumber(serverPort)){
                mySystemInfo.serverPortNum =  serverPort;
            }
            else{
                error("Invalid server Port Number entered\n");
                exit(1);
            }
            argCount++;
        }

    }while(argCount < argc);

    // Check the server port number and paxos port number
    if(mySystemInfo.serverPortNum == mySystemInfo.paxosPortNum){
        error("Server Port and Paxos Port Number needs to be different\n");
        exit(1);
    }


    if(paramDebug){
        fprintf(stderr, "Inside the print if\n");
        fprintf(stderr, "Enetered paxos port number is: %ld\n", mySystemInfo.paxosPortNum);
        fprintf(stderr, "Entered host file name is: %s\n",
                mySystemInfo.hostFileName.c_str());
        fprintf(stderr, "Enetered server port number is: %ld\n", mySystemInfo.serverPortNum);
        fflush(stderr);
    }

    struct in_addr **ipAddr;
    struct hostent* he;
    FILE* fp = fopen(mySystemInfo.hostFileName.c_str(), "r");

    if(paramDebug){
        fprintf(stderr, "total process: %d\n", mySystemInfo.totalNumProcess);
        fflush(stderr);
    }

    // Get self hostname
    char myName[100];
    gethostname(myName, 100);
    mySystemInfo.selfHostName = myName;


    for(int i = 0; i<mySystemInfo.totalNumProcess; i++){

        if(fp == NULL){
            error("Unable to open the hostfile\n");
            exit(1);
        }

        // Get the ipaddress of all the hosts.
        char currHost[100];

        if(fgets(currHost, 100, fp) != NULL){
            mySystemInfo.hostNames.push_back(currHost);
            if(mySystemInfo.hostNames[i].rfind('\n') != string::npos){
                mySystemInfo.hostNames[i].erase(--(mySystemInfo.hostNames[i].end()));
            }

        }

        if((he = gethostbyname(mySystemInfo.hostNames[i].c_str())) == NULL){
            printf("Unable to get the ip address of the host: %s\n",
                    currHost);
            fflush(stdout);
            exit(1);
        }

        //Store the ip address
        ipAddr = (struct in_addr**)he->h_addr_list;

        if(mySystemInfo.selfHostName.compare(mySystemInfo.hostNames[i]) == 0){
            string currentIP = inet_ntoa(*ipAddr[XINU]);

            if(currentIP.find("127") == 0){
                mySystemInfo.hostIPAddr.push_back(inet_ntoa(*ipAddr[VM]));
            }
            else{
                mySystemInfo.hostIPAddr.push_back(inet_ntoa(*ipAddr[XINU]));
            }
        }
        else{
            mySystemInfo.hostIPAddr.push_back(inet_ntoa(*ipAddr[XINU]));
        }

        // update the map
        mySystemInfo.ipToID.insert(pair<string, int>(mySystemInfo.hostIPAddr[i], i+1));
        mySystemInfo.hostNameToIP.insert(
                pair<string, string>(mySystemInfo.hostNames[i],
                    mySystemInfo.hostIPAddr[i]));

        if (paramDebug)
            fprintf(stderr, "Host name: %s, Ip address: %s\n",
                    mySystemInfo.hostNames[i].c_str(),
                    mySystemInfo.hostIPAddr[i].c_str());
            fflush(stderr);
    }
    fclose(fp);

    // Set the selfID
    myStateVariable.myServerID =
        mySystemInfo.ipToID[mySystemInfo.hostNameToIP[mySystemInfo.selfHostName]];

    if(paramDebug){
        fprintf(stderr, "Self hostname is %s\n", mySystemInfo.selfHostName.c_str());
        fprintf(stderr, "Self IP Address is %s\n",
                mySystemInfo.hostNameToIP[mySystemInfo.selfHostName].c_str());
        fprintf(stderr, "Self ID is %d\n", myStateVariable.myServerID);
        fflush(stderr);
    }

    /********************************************************************************/
    /*******   UDP PORTS    *******/
    struct sockaddr_in rcvrAddrUDP;
    struct sockaddr_in senderProcAddrUDP;
    struct sockaddr_in myInfoUDP;

    // To store the address of the process from whom a message is received
    memset((char*)&senderProcAddrUDP, 0, sizeof(senderProcAddrUDP));
    socklen_t senderLenUDP = sizeof(senderProcAddrUDP);

    // store the info for sending the message to a process. rcvrAddr will have all the info
    // about the process whom we are sending the message
    memset((char*)&rcvrAddrUDP, 0, sizeof(rcvrAddrUDP));
    rcvrAddrUDP.sin_family = AF_INET;
    rcvrAddrUDP.sin_port = htons(mySystemInfo.paxosPortNum);

    // Store the info to bind receiving port with the socket.
    memset((char*)&myInfoUDP, 0, sizeof(myInfoUDP));
    myInfoUDP.sin_family = AF_INET;
    myInfoUDP.sin_port = htons(mySystemInfo.paxosPortNum);
    myInfoUDP.sin_addr.s_addr = htonl(INADDR_ANY);

    // Open a UDP sender socket to send the messages
    if((mySystemInfo.myUDPSendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1){
        error("Error when trying to open the UDP socket to send message\n");
        exit(1);
    }

    // Open a UDP receiver socket to receive the messages
    if((mySystemInfo.myUDPRcvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1){
        error("Error when trying to open the receiving socket \n");

        exit(1);
    }

    // bind the UDP receiver socket
    if(bind(mySystemInfo.myUDPRcvSocket, (struct sockaddr*) &myInfoUDP, sizeof(myInfoUDP)) == -1){
        error("Bind failed for receiving socket \n");
        exit(1);
    }


    /*****************************************************************************/
    /******* TCP PORTS ********/
    struct sockaddr_in rcvrAddrTCP;
    struct sockaddr_storage senderProcAddrTCP;
    struct sockaddr_in senderClientAddrTCP;
    struct sockaddr_in myInfoTCP;
    char clientIP[INET6_ADDRSTRLEN];
    int newfd; // newly accpeted socket file descriptor.


    // To store the address of the process from whom a message is received
    memset((char*)&senderProcAddrTCP, 0, sizeof(senderProcAddrTCP));
    socklen_t senderLenTCP = sizeof(senderProcAddrTCP);
    
    // To store the address of the client process from whom a message is received
    memset((char*)&senderClientAddrTCP, 0, sizeof(senderClientAddrTCP));
    socklen_t senderClientLenTCP = sizeof(senderClientAddrTCP);

    // store the info for sending the message to a process. rcvrAddr will have all the info
    // about the process whom we are sending the message
    memset((char*)&rcvrAddrTCP, 0, sizeof(rcvrAddrTCP));
    rcvrAddrTCP.sin_family = AF_INET;
    rcvrAddrTCP.sin_port = htons(mySystemInfo.serverPortNum);

    // Store the info to bind receiving port with the socket.
    memset((char*)&myInfoTCP, 0, sizeof(myInfoTCP));
    myInfoTCP.sin_family = AF_INET;
    myInfoTCP.sin_port = htons(mySystemInfo.serverPortNum);
    myInfoTCP.sin_addr.s_addr = htonl(INADDR_ANY);


    // SELECT MECHANISM: We need to use select here as we can listen from multiple process.
    // 1) We can listen from a client using the tcp connection
    // 2) We can listen from the servers using the UDP connection
    // We need to handle each situation separately

    fd_set read_fds; // set of read fds.
    int fdmax; // keep track of the maximum fd.


    // Clear the master and temp sets
    FD_ZERO(&mySystemInfo.master);
    FD_ZERO(&read_fds);

    // Open the listener TCP socket
    if((mySystemInfo.myTCPListenSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        error("Error when trying to open the listener socket for TCP Connections\n");
        exit(1);
    }

    int yes = 1;
    setsockopt(mySystemInfo.myTCPListenSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    // Bind the TCP socket
    if(bind(mySystemInfo.myTCPListenSocket, (struct sockaddr*) &myInfoTCP, sizeof(myInfoTCP)) == -1){
        error("Bind failed for TCP Listening socket \n");
        exit(1);
    }


    // Listen for 50 seconds
    if(listen(mySystemInfo.myTCPListenSocket, 50) == -1){
        error("Listen Failed \n");
        exit(1);
    }

    // Add the listener to the master list
    FD_SET(mySystemInfo.myTCPListenSocket, &mySystemInfo.master);

    // Add the UDP socket to the master list too
    FD_SET(mySystemInfo.myUDPRcvSocket, &mySystemInfo.master);

    // Keep track of the biggest file descriptor
    if(mySystemInfo.myTCPListenSocket > mySystemInfo.myUDPRcvSocket){
        fdmax = mySystemInfo.myTCPListenSocket;
    }
    else
    {
        fdmax = mySystemInfo.myUDPRcvSocket;
    }


    // Set the progress timer
    gettimeofday(&myTimers.progressTimers, NULL);
    mySystemInfo.isProgressTimerSet = true;

    int selectReturnValue = -1;
    //Main Infinite Loop
    for(;;){

        if(errorDebug){
            printf("############################################################\n");
            printf("For Loop started\n");
            fflush(stdout);
        }

        read_fds = mySystemInfo.master;

        // Check for progress timer expiry
        struct timeval end;
        struct timeval selectTimer;
        gettimeofday(&end, NULL);
        long diff = ((end.tv_sec * 1000000 + end.tv_usec) - 
                        ((myTimers.progressTimers.tv_sec * 1000000) + (myTimers.progressTimers.tv_usec)));

        if(errorDebug){
            fprintf(stderr, "Diff in time:%ld\n", diff);
            fflush(stderr);
        }
        if(diff > mySystemInfo.progressTimerExpiry){
            error("Progress timer has expired\n");
            selectTimer.tv_sec = 0;
            selectTimer.tv_usec = 0;
            //gettimeofday(&myTimers.progressTimers.tv_sec, NULL);
            //mySystemInfo.isProgressTimerSet = true;
        }
        else{
            diff = mySystemInfo.progressTimerExpiry - diff;
            long int sec  =  diff / 1000000; 
            long int msec = diff - (sec * 1000000);
            selectTimer.tv_sec = sec;
            selectTimer.tv_usec = msec;

        }

        if(errorDebug){
            fprintf(stderr, "Diff in time:%ld\n", diff);
            fprintf(stderr, "progressTimerRange: %ld\n", mySystemInfo.progressTimerExpiry);
            fprintf(stderr, "Select Timer sec:%ld and msec:%ld\n", selectTimer.tv_sec, selectTimer.tv_usec);
            fflush(stderr);
        }

        if(mySystemInfo.isProgressTimerSet){

            if(errorDebug){
                fprintf(stderr, "Progress Timer is set\n");
                fflush(stderr);
            }
            selectReturnValue = select(fdmax + 1, &read_fds, NULL, NULL, &selectTimer);
        }
        else
        {   
            if(errorDebug){
                fprintf(stderr, "Progress Timer is not set\n");
                fflush(stderr);
            }
            selectReturnValue = select(fdmax + 1, &read_fds, NULL, NULL, NULL);
        }

        if(errorDebug){
            fprintf(stderr, "Seclect return value %d\n", selectReturnValue);
            fflush(stderr);
        }

        if(selectReturnValue == -1){
            error("Select Failed \n");
        }
        else if(selectReturnValue == 0){
            error("Timeout occured inside select\n");
            
            // Timeout happened. So here we will do the leader election.
            mySystemInfo.isProgressTimerSet = false;
            
            shiftToLeaderElection(myViewState.lastAttempted + 1);
        }
        else{

            if(errorDebug){
                fprintf(stderr,"############################################\n");
                fprintf(stderr, "FOR LOOP FOR FDS started\n");
                fflush(stderr);
            }

            // Run through the existing connections for the data to be read
            for(int i = 0; i<=fdmax; i++){

                if(FD_ISSET(i, &read_fds)){ // We got one active fd
                    if(i == mySystemInfo.myTCPListenSocket){

                        // handle new connections
                        if((newfd = accept(mySystemInfo.myTCPListenSocket, 
                                        (struct sockaddr*)&senderProcAddrTCP,
                                        &senderLenTCP)) == -1)
                        {
                            error("Accept failed\n");
                        }
                        else
                        {
                            FD_SET(newfd, &mySystemInfo.master); // add the new client socket into
                            // the master set
                            if(newfd > fdmax){
                                fdmax = newfd;
                            }
                            string currClientIP;
                            currClientIP = inet_ntop(senderProcAddrTCP.ss_family,
                                    get_in_addr((struct sockaddr*)&senderProcAddrTCP),
                                    clientIP, INET_ADDRSTRLEN);
                            if(paramDebug){
                                fprintf(stderr, "New connection from client: %s on socket%d\n",
                                        currClientIP.c_str(), newfd);
                                fflush(stderr);
                            }
                            // Store the current client socket and IP address inside the map
                            //mySystemInfo.myTCPClientSocket[currClientIP] = newfd;
                        }
                    }
                    else if(i == 0){   
                        // Standard IO descriptor is active. I think we should ignore this.
                    }
                    else if(i == mySystemInfo.myUDPRcvSocket){ // UDP message received

                        // Allocate the maximum message size
                        int status;

                        status = recvfrom(mySystemInfo.myUDPRcvSocket, maxMessage, MAXMSGSIZE,
                                MSG_DONTWAIT, (struct sockaddr*) &senderProcAddrUDP, &senderLenUDP);

                        if(status > 0){


                            // Check the message type
                            uint32_t *msgType = (uint32_t*)maxMessage;
                            uint32_t type = *msgType;
                            type = ntohl(type);

                            string senderIP = inet_ntoa(senderProcAddrUDP.sin_addr);

                            if(errorDebug){
                                fprintf(stderr, "Recevived msg:%d of size %d from server:%s\n", type, status, senderIP.c_str());
                                fflush(stderr);
                            }

                            
                            // Need to handle the prepareok message separately
                            if(type == PREPARE_OK_MSG){
                                char* messagePart = NULL;
                                messagePart = (char*)malloc(status);
                                memcpy(messagePart, maxMessage, status);

                                rcvdMessage = (char*)deserializePrepareOk(messagePart);

                                free(messagePart);
                                messagePart = NULL;
                            }
                            else{
                                rcvdMessage = NULL;
                                rcvdMessage = (char*)malloc(status);

                                if(!rcvdMessage){
                                    error("while malloc for recvd message\n");
                                    exit(1);
                                }
                                memcpy(rcvdMessage, maxMessage, status);

                                if(errorDebug){
                                    fprintf(stderr, "Copied the message buffer into rcvdMessage\n");
                                    fflush(stderr);
                                }
                            }

                            // Now we can memset the maxMessage memory
                            //free(maxMessage);
                            //maxMessage = NULL;

                            // Covert the message from network order to host order
                            convertNTOHByteOrder(rcvdMessage, type);


                            if(conflictMessage(rcvdMessage, type)){
                                // Ignore the message
                                // Free the memory
                                error("Conflicting message discarding\n");

                                if(rcvdMessage){
                                    //free(rcvdMessage);
                                    //rcvdMessage = NULL;
                                }
                            }
                            else{

                                handleMessage(rcvdMessage, type);
                            }
                        }
                        else if(status < 0){
                            error("Error while receiving message \n");
                            //free(maxMessage);
                            //maxMessage = NULL;
                        }

                    }
                    else{
                        // handle the data from the TCP Client. Client must have sent an update message
                        // We can get multiple client update at a time. So we have to maintain a map
                        // of TCP client socketfds and ipaddress. This help in sending the response to the
                        // clients for their request message.

                        // Allocate the memory for receivng the client update
                        Client_Update* clientUpdate = NULL;
                        clientUpdate = (Client_Update*) malloc(sizeof(Client_Update));
                        
                        /*if(recvfrom(i, clientUpdate, sizeof(Client_Update), MSG_DONTWAIT,
                                    (struct sockaddr*) &senderClientAddrTCP, &senderClientLenTCP) > 0)
                        */
                        if(recv(i, clientUpdate, sizeof(Client_Update), MSG_DONTWAIT) > 0){

                            getpeername(i, (struct sockaddr*) &senderClientAddrTCP, &senderClientLenTCP);
                            
                            if(errorDebug){
                                fprintf(stderr,"Client add in long form %d\n", senderClientAddrTCP.sin_addr.s_addr);
                                fflush(stderr);
                            }
                            string clientAddr(inet_ntoa(senderClientAddrTCP.sin_addr));

                            
                            // Convert the client update message from network to host order
                            convertNTOHByteOrder(clientUpdate, CLIENT_UPDATE_MSG);

                            // Update the map for client address to id
                            //mySystemInfo.myTCPClientID_IP[clientUpdate->client_id] = clientAddr;
                            mySystemInfo.myTCPClientID_FD[clientUpdate->client_id] = i;
                            
                            if(errorDebug){
                                fprintf(stderr, "Address got %s\n", clientAddr.c_str());
                                fprintf(stderr, "Clientid %d with fd Stored:%d\n", clientUpdate->client_id, 
                                        mySystemInfo.myTCPClientID_FD[clientUpdate->client_id]);
                                fflush(stderr);
                            }
                            // Pass the message to the client update handler
                            clientUpdateHandler(clientUpdate);

                        }
                        else{
                            error("Error while receiving message from client\n");
                        }

                    }

                    


                }// END of FD SET Condition
                
                // Check for the update timer expiry here. If I am not the leader then
                // I will have updates in my queue so forward it to the current leader
                struct timeval endTimeAgain;
                gettimeofday(&endTimeAgain, NULL);

                for(myTimers.myIter = myTimers.updateTimers.begin();
                        myTimers.myIter != myTimers.updateTimers.end();
                        myTimers.myIter++)
                {

                    long myDiff = ((endTimeAgain.tv_sec * 1000000 + endTimeAgain.tv_usec) -
                                    ((myTimers.myIter->second.tv_sec * 100000) + (myTimers.myIter->second.tv_usec)));


                    // Check for which all clientID the update timer has expired
                    if(myDiff > mySystemInfo.updateTimerExpiry){
                    
                        // Restart this timer
                        gettimeofday(&myTimers.myIter->second, NULL);
                    
                        if(myStateVariable.currState == REG_NONLEADER){
                            // Send all the pending updated with this client ID 
                            // to the current leader
                            if(myClientHandle.pendingUpdatesMsgMap[myTimers.myIter->first]){

                                // Send to leader
                                sendToLeader(myClientHandle.pendingUpdatesMsgMap[myTimers.myIter->first],
                                        CLIENT_UPDATE_MSG);
                            }
                        }
                    }
                }
            }// End of FD SET FOR Loop

            if(errorDebug){
                fprintf(stderr, "############################################\n");
                fprintf(stderr, "FOR LOOP FOR FDS ended\n");
                fflush(stderr);
            }

        }
        
        /*
        if(myStateVariable.currState == LEADER_ELECTION && myStateVariable.currState != PREPARE_PHASE){ 
            // send the vcProof Message
            VC_Proof* vcProofMsg = constructVCProofMsg();

            // Send to all servers
            sendMsgToAllServers(vcProofMsg, VC_PROOF_MSG);

            // Free the memory
            if(vcProofMsg){
                free(vcProofMsg);
                vcProofMsg = NULL;
            }
        }
        */
        // Check for VC message timer
        time_t vcEndTime;
        time(&vcEndTime);

        if(difftime(vcEndTime, myTimers.vcProofTimers) > VCPROOF_TIMER_EXPIRY){

            // send the vcProof Message
            VC_Proof* vcProofMsg = constructVCProofMsg();

            // Send to all servers
            sendMsgToAllServers(vcProofMsg, VC_PROOF_MSG);

            // Free the memory
            if(vcProofMsg){
                free(vcProofMsg);
                vcProofMsg = NULL;
            }

            // Restart the timer
            time(&myTimers.vcProofTimers);
        }

        if(errorDebug){
            fprintf(stderr, "############################################################\n");
            fprintf(stderr, "For Loop ended\n");
            fflush(stderr);
        }

    }// END of infinite loop

    return 0;
}// End of main
