#include "client.h"

void error(const char msg[])
{
    if(errorDebug){
        fprintf(stderr,"Error: %s",msg);
    }
}


void printUsage(){
    printf("Usage: client -s server_port -f command_file -i client_id -h hostfile\n");
    exit(1);
}


bool verifyPortNumber(long portNumber){
    error("Entered function verifyPortNumber\n");

    if(portNumber < 1024 || portNumber >65535){
        return false;
    }
    return true;
}


bool verifyFileName(string fileName, int *count){
    error("Entered function verifyCommandFileName\n");

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

    *count = numLine;

    return true;
}


bool verifyHostFile(string fileName){
    error("Entered function verifyHostFile\n");

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

    return true;
}




void convertHTONByteOrder(Client_Update* message){

    error("Entered the function convertHTONByteOrder\n");

    message->type = htonl(message->type);
    message->client_id = htonl(message->client_id);
    message->server_id = htonl(message->server_id);
    message->timestamp = htonl(message->timestamp);
    message->update = htonl(message->update);

}

void convertNTOHByteOrder(Client_Update* message){

    error("Entered the function convertHTONByteOrder\n");

    message->type = ntohl(message->type);
    message->client_id = ntohl(message->client_id);
    message->server_id = ntohl(message->server_id);
    message->timestamp = ntohl(message->timestamp);
    message->update = ntohl(message->update);

}  


int main(int argc, char* argv[])
{
    int argCount = 1;
    long serverPort = -1;
    string fileName;
    string hostfile;
    int clientID = -1;
    int updateCount = 0;
    vector<int> updateList;
    vector<string> hostIP;
    vector<string> hostNameList;
    map<string, int> hostNameToID;

    // Implement the command line option parser
    if(argc < 9)
    {
        // Insufficient number of options entered
        error("Invalid command entered\n");
        printUsage();
    }

    do{

        // Get the server port number
        if(strcmp(argv[argCount], "-s") == 0){
            serverPort = atoi(argv[++argCount]);
            if(!verifyPortNumber(serverPort)){
                error("Invalid server Port Number entered\n");
                exit(1);
            }
            argCount++;
        }

        // Get the commandfile name
        if(strcmp(argv[argCount], "-f") == 0){
            fileName = argv[++argCount];
            if(!verifyFileName(fileName, &updateCount)){
                error("Invalid command file name entered\n");
                exit(1);
            }
            argCount++;
        }

        // Get the client id
        if(strcmp(argv[argCount], "-i") == 0){
            clientID = atoi(argv[++argCount]);
            argCount++;
        }

        // Get the hostfile
        if(strcmp(argv[argCount], "-h") == 0){
            hostfile = argv[++argCount];
            if(!verifyHostFile(hostfile)){
                error("Invalid hostfile name entered\n");
                exit(1);
            }
            argCount++;
        }

    }while(argCount < argc);

    if(paramDebug){
        fprintf(stderr, "Inside the print if\n");
        fprintf(stderr, "Enetered server port number is: %ld\n", serverPort);
        fprintf(stderr, "Entered command file name is: %s\n",
                fileName.c_str());
        fprintf(stderr, "Enetered client ID is: %d\n", clientID);
        fprintf(stderr, "Number of updates in file: %d\n", updateCount);
    }

    struct in_addr **ipAddr;
    struct hostent* he;
    FILE* fp = fopen(fileName.c_str(), "r");

    if(fp == NULL){
        error("Unable to open the command file\n");
        exit(1);
    }


    char hostname[100];
    int update;

    while(fscanf(fp,"%s %d",hostname, &update) == 2){
        if(paramDebug){
            fprintf(stderr, "Hostname: %s and update: %d\n",hostname, update);
        }

        if((he = gethostbyname(hostname)) == NULL){
            fprintf(stderr, "Unable to get the ip address of the host: %s\n",
                    hostname);
            continue; // Neglect this update and move to next one
        }

        //Store the ip address
        ipAddr = (struct in_addr**)he->h_addr_list;

        // Store the update and hostIP
        updateList.push_back(update);

        string currentIP = inet_ntoa(*ipAddr[XINU]);

        if(currentIP.find("127") == 0){
            hostIP.push_back(inet_ntoa(*ipAddr[VM]));
            if(paramDebug){
                fprintf(stderr, "Host IP: %s\n", inet_ntoa(*ipAddr[VM]));
            }
        }
        else{
            hostIP.push_back(inet_ntoa(*ipAddr[XINU]));

            if(paramDebug){
                fprintf(stderr, "Host IP: %s\n", inet_ntoa(*ipAddr[XINU]));
            }
        }

        hostNameList.push_back(hostname);

    }

    if(feof(fp))  
    {            
        error("EOF");     
    }  
    else  
    {  
        error("CANNOT READ");  
    }

    fclose(fp);

    // Create a map of hostname to id
    ifstream myStream(hostfile.c_str());

    string currHost;
    int hostID = 1;

    if(myStream.is_open()){
        while(myStream.good()){

            getline(myStream, currHost);

            if(!currHost.empty()){
                hostNameToID[currHost] = hostID++;
                if(paramDebug){
                    fprintf(stderr, "Server ID: %d and serverName: %s\n", hostNameToID[currHost], currHost.c_str());
                }
            }
        }
    }
    
    int mySocket;
    struct sockaddr_in serv_addr;

    

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(serverPort);


    for(int i=0; i<updateCount; i++){
        if(inet_aton(hostIP[i].c_str(), &serv_addr.sin_addr) == 0){
            error("INET_ATON failed\n");

            if(paramDebug){
                fprintf(stderr, "Address storing in sockaddr_in failed for %s\n", 
                                hostIP[i].c_str());
                fprintf(stderr, "Skipping sending his update %d\n", updateList[i]);
            }

            continue;
        }
        
        mySocket = socket(AF_INET, SOCK_STREAM, 0);

        if(mySocket < 0){
            error("Error while opening socket\n");
            exit(1);
        }

        if(connect(mySocket,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        {

            if(paramDebug){
                fprintf(stderr, "Error in connecting for %s\n", 
                        hostIP[i].c_str());
                fprintf(stderr, "Skipping sending his update %d\n", updateList[i]);
            }

            continue;
        }

        // Create the message
        Client_Update* message = (Client_Update*)malloc(sizeof(Client_Update));
        message->type = CLIENT_UPDATE_MSG;
        message->client_id = clientID;
        message->server_id = hostNameToID[hostNameList[i].c_str()];
        message->timestamp = i+1;
        message->update = updateList[i];

        // Convert the message from host to network order
        convertHTONByteOrder(message);

        fprintf(stderr,"%d: Sending update %d to server %d\n", clientID, updateList[i], hostNameToID[hostNameList[i].c_str()]);

        // Send the message
        if(sendto(mySocket, message, sizeof(Client_Update), 0, 
             (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0){

            fprintf(stderr, "%d: Unable to send update %d to the server %d\n", clientID, updateList[i], hostNameToID[hostNameList[i].c_str()]);

            continue;
        }

        // wait for receiving the message
        // SORABH: Need to implement aftrer acking the response message format
        char response[50];
        int status = -1;

        status = recv(mySocket, response, 50, 0);
        if(status > 0){
            
            fprintf(stderr, "%d: Update %d send to server %d is executed\n", clientID, updateList[i], hostNameToID[hostNameList[i].c_str()]);
        }
        else if(status == 0){
            fprintf(stderr, "%d: Connection for update %d is closed by server %d\n", clientID, updateList[i], hostNameToID[hostNameList[i].c_str()]);
        }
        else{
            fprintf(stderr, "%d:Some other error happened in receive for update %d by server %d\n",
                            clientID, updateList[i], hostNameToID[hostNameList[i].c_str()]);
        }
        
        // Close the socket as we are opening each time
        close(mySocket);
    }

    return 0;
}
