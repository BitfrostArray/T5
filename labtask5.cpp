// Lab Task
// Run with 6 peers
// Initially,
// - P0 to P4 is in a multicast group
// - P5 is standalone and will join the group later
// - P0 will build the group view V and send it to other member processes
//
// Each process in the group multicast messages to the current group member based on group view
// P5 will join the group after a while

// We want to ensure that
// (1) either every non-faulty group members receive the message or none of them receive the message
// (2) the messages should be received by the non-faulty processes in the same order

// Assumption : (1) No processes crash during build view phase and multicast phase

#include <mpi.h>
#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <cstring>

#define BUILD_TAG		0
#define GV_TAG			1
#define MSG_TAG			2
#define VIEW_REQ		111
#define VIEW_ACK		112

using namespace std;

char V[6] = {'F','F','F','F','F','F'};	//array to store Group View
int world_size;
int data[6] = {0,0,0,0,0,0};	// array to store incoming multicast message data
int p = 0;
int Grank;
//non-blocking receive function with auto time out
bool nonBlockingRecv (int &re, int destination, MPI_Status &stat, int tag){
	int reply;
	int flag, count;
	MPI_Request req;
	MPI_Irecv (&reply, 1, MPI_INT, destination, tag, MPI_COMM_WORLD, &req);
	flag = 0;
	count = 0;
	do {
		//wait for 10ms before retry
		usleep (10000);
		count++;
		//cancel if it takes longer than 100ms
		if (count > 10) {
			MPI_Cancel (&req);
			break;
		}
		//Test to see if msg arrive
		MPI_Test (&req, &flag, &stat);
	} while (!flag);
	//if receive msg
	if (flag) {
		re = reply;
		return true;
	}
	else
		return false;
}

// Multicast function use by the process when sending messages
void multicast (int r){
	int dest;
	int msg, reply;
	MPI_Status status;
	MPI_Request req;

	msg = rand() % 100;

	// a copy of msg to myself
	data[r] = msg;
	// data[p] = msg;
	p++;

	for (int i=0; i<world_size; i++) {
		//Send message to other member process
		if (i != r && V[i] == 'T') {
			dest = i;
			MPI_Send (&msg, 1, MPI_INT, dest, MSG_TAG, MPI_COMM_WORLD);
		}
	}

   // cout << "[P" << Grank << "] Sent ";
   // for (size_t i = 0; i < p; i++) {
   //    cout << data[i] << " ";
   // }
   // cout << endl;
}

int main (int argc, char* argv[]) {
   MPI_Init (NULL, NULL);

   MPI_Comm_size (MPI_COMM_WORLD, &world_size);
   int rank;
   MPI_Comm_rank (MPI_COMM_WORLD, &rank);
   Grank = rank;
	int dest;
	int msg, reply;
	int flag;
	MPI_Status status;
	MPI_Request req;
	int count = 0;

	srand (rank*10);

	// P0 build Group View (V) and send to other processes (except P5, which will only available later)
	if (rank == 0) {
		cout << "\033[3" << rank+1 << "m" << "[P" << rank << "] Start" << endl;
		for (int i=1; i<world_size; i++) {
			dest = i;
			msg = VIEW_REQ;
			//Send VIEW_REQ to other process
			MPI_Send (&msg, 1, MPI_INT, dest, BUILD_TAG, MPI_COMM_WORLD);
			if (nonBlockingRecv (reply, MPI_ANY_SOURCE, status, BUILD_TAG)) {
				//if a process reply with VIEW_ACK, add it as member
				if (reply == VIEW_ACK) {
					V[status.MPI_SOURCE] = 'T';
				}
			}
		}
		//P0 itself is member
		V[0] = 'T';

		// send group view V to other process
		for (int i=1; i<world_size; i++)
			MPI_Send (&V, 6, MPI_CHAR, i, GV_TAG, MPI_COMM_WORLD);
	}

	// P1 to P5
	else {
		// P5 is assume to be joining later, missing the build view stage
		if (rank == 5) {
			sleep (2);
         cout << "\033[3" << rank+1 << "m" << "[P" << rank << "] Is active" << endl;
         MPI_Recv (&msg, 1, MPI_INT, 0, BUILD_TAG, MPI_COMM_WORLD, &status);
   		if (msg == VIEW_REQ) {
   			reply = VIEW_ACK;
   			MPI_Send (&reply, 1, MPI_INT, 0, BUILD_TAG, MPI_COMM_WORLD);
   		}
			// sleep (2);
			MPI_Recv(&V, 6, MPI_CHAR, 0, GV_TAG, MPI_COMM_WORLD, &status);

			cout << "\033[3" << rank+1 << "m" << "[P" << rank << "] View : ";
			for (int i=0; i<world_size; i++)
				if (V[i] == 'T')
					cout << "\033[3" << rank+1 << "m" << i << " ";
			cout << endl;

			multicast (rank);
         // cout << "\033[3" << rank+1 << "m" << "[P" << rank << "] Finished multicasting" << endl;
			goto join;
		}

		cout << "\033[3" << rank+1 << "m" << "[P" << rank << "] Start" << endl;

		// if receive build view request from P0, reply with VIEW_ACK
		MPI_Recv (&msg, 1, MPI_INT, 0, BUILD_TAG, MPI_COMM_WORLD, &status);
		if (msg == VIEW_REQ) {
			reply = VIEW_ACK;
			MPI_Send (&reply, 1, MPI_INT, 0, BUILD_TAG, MPI_COMM_WORLD);
		}
		// receive new group view, V, from P0
		MPI_Recv (&V, 6, MPI_CHAR, 0, GV_TAG, MPI_COMM_WORLD, &status);
	}

	// All processes except P5
	cout << "\033[3" << rank+1 << "m" << "[P" << rank << "] View : ";
	for (int i=0; i<world_size; i++)
		if (V[i] == 'T')
			cout << "\033[3" << rank+1 << "m" << i << " ";
	cout << endl;

	//wait for random amount of time before multicast message - to simulate the unpredictable network latency
	usleep (rand()%10000);
	multicast (rank);
   // cout << "\033[3" << rank+1 << "m" << "[P" << rank << "] Finished multicasting" << endl;


join:		//P5 join here and won't receive the messages since P5 is not in the V yet
	do {
		if (nonBlockingRecv (msg, MPI_ANY_SOURCE, status, MSG_TAG)) {
			// store the received msg to data[]
         // data[0] is the node's sent data
         data[status.MPI_SOURCE] = msg;
         // if (status.MPI_SOURCE > rank) {
         //    data[status.MPI_SOURCE] = msg;
         // } else {
         //    data[status.MPI_SOURCE+1] = msg;
         // }

			// data[p] = msg;
			p++;
		}
		else {
			count++;
			if (count > 15)
				break;
		}
	} while (1);
   // cout << "\033[3" << rank+1 << "m" << "[P" << rank << "] Finished receiving" << endl;

   //Update if rank is 0
	if (V[rank] == 'T') {
		if (nonBlockingRecv (reply, MPI_ANY_SOURCE, status, BUILD_TAG) && rank == 0) {
			if (reply == VIEW_ACK) {
				V[status.MPI_SOURCE] = 'T';
				cout << "\033[3" << rank+1 << "m" << "[P" << status.MPI_SOURCE << "] Is added to GV LATE\n > Resending GV to all participating nodes." << endl;
				for (int i=1; i<world_size; i++){
					MPI_Send (&V, 6, MPI_CHAR, i, GV_TAG, MPI_COMM_WORLD);
				}
			}
		} else {
   		MPI_Recv (&V, 6, MPI_CHAR, 0, GV_TAG, MPI_COMM_WORLD, &status);
      }

		for (size_t i = 0; i < world_size; i++) {
			if (i != rank && V[i] == 'T') {
				dest = i;
				MPI_Isend(&data[rank], 1, MPI_INT, dest, MSG_TAG, MPI_COMM_WORLD, &req);
			}
		}
	}

   // Everyone print the messages they receive

   cout << "\033[3" << rank+1 << "m" << "[P" << rank << "] ";
   for (int i=0; i<world_size; i++){
      cout << "\033[3" << rank+1 << "m" << data[i] << " ";
   }
   cout << endl;

   cout << "\033[0m";
   MPI_Barrier (MPI_COMM_WORLD);

   // Finalize the MPI environment.
   MPI_Finalize();
}
