// PageTable.c ... implementation of Page Table operations
// COMP1521 17s2 Assignment 2
// Written by John Shepherd, September 2017

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include "Memory.h"
#include "Stats.h"
#include "PageTable.h"

// Symbolic constants

#define NOT_USED 0
#define IN_MEMORY 1
#define ON_DISK 2

// PTE = Page Table Entry

typedef struct {
   char status;      // NOT_USED, IN_MEMORY, ON_DISK
   char modified;    // boolean: changed since loaded
   int  frame;       // memory frame holding this page
   int  accessTime;  // clock tick for last access
   int  loadTime;    // clock tick for last time loaded
   int  nPeeks;      // total number times this page read
   int  nPokes;      // total number times this page modified
   // TODO: add more fields here, if needed ...
} PTE;

// A structure to represent a queue
typedef struct Queue {
    int front;
    int last;
    int size;
    unsigned capacity;
    int* pages;
}Queue_Node;

typedef struct Node  {
	int data;
	Node next;
	Node prev;
} Node;



// The virtual address space of the process is managed
//  by an array of Page Table Entries (PTEs)
// The Page Table is not directly accessible outside
//  this file (hence the static declaration)

static PTE *PageTable;      // array of page table entries
static int  nPages;         // # entries in page table
static int nFrames;
static int  replacePolicy;  // how to do page replacement
static int  fifoList;       // index of first PTE in FIFO list
static int  fifoLast;       // index of last PTE in FIFO list
static Queue_Node *lru_list;
static Node head;
static Node fifo_tail;


// Forward refs for private functions

static int findVictim(int);
struct Queue* createQueue(unsigned);
int isFull(struct Queue*);
int isEmpty(struct Queue*);
int dequeue(struct Queue*);
void enqueue(struct Queue*, int); 

// initPageTable: create/initialise Page Table data structures

void initPageTable(int policy, int np)
{
   PageTable = malloc(np * sizeof(PTE));
   if (PageTable == NULL) {
      fprintf(stderr, "Can't initialise Memory\n");
      exit(EXIT_FAILURE);
   }
   replacePolicy = policy;
   nPages = np;
   fifoList = 0;
   fifoLast = nPages-1;
   for (int i = 0; i < nPages; i++) {
      PTE *p = &PageTable[i];
      p->status = NOT_USED;
      p->modified = 0;
      p->frame = NONE;
      p->accessTime = NONE;
      p->loadTime = NONE;
      p->nPeeks = p->nPokes = 0;
   }

   lru_list = createQueue(nPages*nFrames);
   fifo_list = createQueue (nPages*nFrames);
}

void updatePageTable(int pno, int fno, int time){
    PTE *p = &PageTable[pno];
    
    p->status =  IN_MEMORY;
    p->modified = 0;
    p->frame = fno;
    p->loadTime= time;
    p->accessTime = time;

    //when loaded into Memory, a new node is 'enqueued' to the Queue
    enqueue(fifo_list, pno);

    //when loaded into Memory, a new node is inserted at the head of doubly linked list
    InsertAtHead(pno);

}

void updateVictimTable(int vno, int time){
    PTE *p = &PageTable[vno];
    
    p->status =  ON_DISK;
    p->modified = 0;
    p->frame = NONE;
    p->loadTime = NONE;
    p->accessTime = NONE;

    deleteItem(vno);
}

// requestPage: request access to page pno in mode
// returns memory frame holding this page
// page may have to be loaded
// PTE(status,modified,frame,accessTime,nextPage,nPeeks,nWrites)

int requestPage(int pno, char mode, int time){
   if (pno < 0 || pno >= nPages) {
      fprintf(stderr,"Invalid page reference\n");
      exit(EXIT_FAILURE);
   }
   PTE *p = &PageTable[pno];
   int fno; // frame number
   switch (p->status) {
   case NOT_USED:
   case ON_DISK:
      // TODO: add stats collection
      fno = findFreeFrame();
      if (fno == NONE) {
         int vno = findVictim(time);
#ifdef DBUG
         printf("Evict page %d\n",vno);
#endif
         // TODO:
         // if victim page modified, save its frame
         PTE *p = &PageTable[vno];
         if(p->modified){
            saveFrame(p->frame);
         }

         // collect frame# (fno) for victim page
         fno = p->frame;
         saveFrame(fno);
         // update PTE for victim page
         // - new status
         // - no longer modified
         // - no frame mapping
         // - not accessed, not loaded
         updateVictimTable(vno,time);         
      }
      printf("Page %d given frame %d\n",pno,fno);
      // TODO:
      // load page pno into frame fno
      loadFrame(fno,pno,time);
      // update PTE for page
      // - new status
      // - not yet modified
      // - associated with frame fno
      // - just loaded
      updatePageTable(pno,fno,time);

      break;
   case IN_MEMORY:
   PTE *p = &PageTable[pno];    
   default:
      fprintf(stderr,"Invalid page status\n");
      exit(EXIT_FAILURE);
   }
   if (mode == 'r')
      p->nPeeks++;
   else if (mode == 'w') {
      p->nPokes++;
      p->modified = 1;
   }
   p->accessTime = time;
   InsertAtHead(pno);
   deleteItem(pno);
   
   return p->frame;
}

// findVictim: find a page to be replaced
// uses the configured replacement policy

static int findVictim(int time)
{
   int victim = NONE;
   switch (replacePolicy) {

   case REPL_LRU:
      //basic code
      /*victim = NONE;
      int temp_time = time;
      for (int i = 0; i < nPages; i++) {
         PTE *p = &PageTable[i];
         if(p->accessTime != NONE){
            if(p->accessTime < temp_time ){
                temp_time = p->accessTime;
                victim = i;
            }
        }
      }*/

      //****************************************************************
      //lru is implemented using a doubly linked list
      //every accesstime the node assosciated with the page 
      //is inserted at the head of list
      //and the copy of the node in the previous position  is deleted
      //thus victim is always the tail of the list
      victim = tail->data;
      deleteTail();


   case REPL_FIFO:

      //basic code
      /*victim = NONE;
      temp_time = time;
      for (int i = 0; i < nPages; i++) {
         PTE *p = &PageTable[i]; 
         if (p->loadTime != NONE){       
            if(p->loadTime < temp_time ){
                temp_time = p->loadTime;
                victim = i;
            }
        }
      }*/
      
      //*****************************************************************
      //fifo-list maintains a queue based on loadTime
      //the dequeue function gives the very first inserted node
      //this is ultimately the first loaded pno
      victim = dequeue(fifo_list);

   
   case REPL_CLOCK:
       return 0;
   }
   return victim;
}

// showPageTableStatus: dump page table
// PTE(status,modified,frame,accessTime,nextPage,nPeeks,nWrites)

void showPageTableStatus(void)
{
   char *s;
   printf("%4s %6s %4s %6s %7s %7s %7s %7s\n",
          "Page","Status","Mod?","Frame","Acc(t)","Load(t)","#Peeks","#Pokes");
   for (int i = 0; i < nPages; i++) {
      PTE *p = &PageTable[i];
      printf("[%02d]", i);
      switch (p->status) {
      case NOT_USED:  s = "-"; break;
      case IN_MEMORY: s = "mem"; break;
      case ON_DISK:   s = "disk"; break;
      }
      printf(" %6s", s);
      printf(" %4s", p->modified ? "yes" : "no");
      if (p->frame == NONE)
         printf(" %6s", "-");
      else
         printf(" %6d", p->frame);
      if (p->accessTime == NONE)
         printf(" %7s", "-");
      else
         printf(" %7d", p->accessTime);
      if (p->loadTime == NONE)
         printf(" %7s", "-");
      else
         printf(" %7d", p->loadTime);
      printf(" %7d", p->nPeeks);
      printf(" %7d", p->nPokes);
      printf("\n");
   }
}




//********************* Queue ______ Functions *******************************
 
// function to create a queue of given capacity. 
// It initializes size of queue as 0
Queue_Node createQueue(unsigned capacity)
{
    Queue_Node queue = (Queue_Node) malloc(sizeof(struct Queue));
    queue->capacity = capacity;
    queue->front = queue->size = 0; 
    queue->last = capacity - 1; 
    queue->pages = (int*) malloc(queue->capacity * sizeof(int));
    return queue;
}
 
// Queue is full when size becomes equal to the capacity 
int isFull(Queue_Node queue)
{  return (queue->size == queue->capacity);  }
 
// Queue is empty when size is 0
int isEmpty(Queue_Node queue)
{  return (queue->size == 0); }
 
// Function to add an item to the queue.  
// It changes last and size
void enqueue(Queue_Node queue, int item)
{
    if (isFull(queue))
        return;
    queue->last = (queue->last + 1)%queue->capacity;
    queue->pages[queue->last] = item;
    queue->size = queue->size + 1;
}
 
// Function to remove an item from queue. 
// It changes front and size
int dequeue(Queue_Node queue)
{
    if (isEmpty(queue))
        return INT_MIN;
    int item = queue->pages[queue->front];
    queue->front = (queue->front + 1)%queue->capacity;
    queue->size = queue->size - 1;
    return item;
}

//***************Doubly_______Linked______List _________Functions*********************************

//Creates a new Node and returns pointer to it. 
Node newNode(int item) {
	Node newNode  = (Node)malloc(sizeof(struct Node));
	newNode->data = item;
	newNode->prev = NULL;
	newNode->next = NULL;
	return newNode;
}

//Inserts a Node at head of doubly linked list
void InsertAtHead(int item) {
	Node newNode = newNode(item);
	if(head == NULL) {
		head = newNode;
		return;
	}
	head->prev = newNode;
	newNode->next = head; 
	head = newNode;
}

void deleteTail(){
    Node toDelete;
    if(tail == NULL){
       return;
    } else {
        toDelete = tail;

        tail = tail->prev; // Move last pointer to 2nd last node
        tail->next = NULL; // Remove link to of 2nd last node with last node
        free(toDelete);       // Delete the last node
    }
}

void deleteItem(int item) { 
   Node currP;
   Node tmp;
   for (currP = head; currP != NULL; currP = currP->next) {
      if (currP->data == item) {  
         if (currP->prev == NULL) { /* Remove from beginning */
            head = currP->next;
         } else if(currP->next == NULL) { /* Remove from end */
            deleteTail();
         } else { /* Remove from middle */
            tmp = currP->prev;
            tmp->next = currP->next;

            tmp = currP->next;
            tmp->prev = currP->prev;
         }
         free(currP) 
      }
  }
}
