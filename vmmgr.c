#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIZE 100

/* structures for virtual addresses */
typedef struct address
{
  uint16_t page_offset : 8; /*  page size of 2^8 */
  uint16_t page_num: 8; /*  16 - 8 = 8 */
} Address;

typedef union addressUnion 
{
  Address bitfield;
  uint16_t ul;
} AddressUnion;

typedef struct tlbEntry
{
  int page_number;
  int frame_number;
  char altered; /* either u for unaltered or a for altered */
  int LRU_number; /* number with how long ago a TLB entry was used*/
} TLBEntry;

typedef struct pageTableEntry
{
  int page_number;
  int frame_number;
  char valid_invalid_bit; /* v for valid and i for invalid */ 
} PageTableEntry;

typedef struct frame
{
  int frame_number;
  char bytes[256];
} Frame;

typedef struct page
{
  char is_free; /* f for free, n for not free */
} Page;

/* global variables */
TLBEntry TLB[16];
PageTableEntry PageTable[256];
Frame physical_memory[256]; /* 65536 bytes */ 
Page freePageList[256];

int num_TLB_entries = 0;
int num_page_faults = 0;
int num_TLB_hits = 0;
int num_addresses = 0;

/* function declarations */
void translateAddress(int address, int pageNumber, int offset);
char* consultTLB(int pageNumber, int offset);
char* consultPageTable(int pageNumber, int offset);

int main(int argc, char** argv)
{
  /* get command line arguments */
  if (argc != 2)
  {
    printf("You must provide the name of the program and the file to be read in, nothing more or less.\n");
    return -1;
  }

  /* initialize arrays */
  int i = 0;
  for (i = 0; i < 16; i++)
  {
    TLBEntry t;
    t.frame_number = 0;
    t.page_number = 0;
    t.altered = 'u';
    t.LRU_number = 0;
    TLB[i] = t;
  }
  for (i = 0; i < 256; i++)
  {
    /* page table starts with all invalid */
    PageTableEntry p;
    p.page_number = 0;
    p.frame_number = 0;
    p.valid_invalid_bit = 'i';
    PageTable[i] = p;
    /* physical memory frames */
    physical_memory[i].frame_number = i;
    /* free page list */
    freePageList[i].is_free = 'f';
  }

  /* read file contents */
  FILE* in;
  char task[SIZE];

  in = fopen(argv[1],"r");

  if (in == NULL)
  {
    printf("Could not open %s\n" ,argv[1]);
    exit(1);
  }
  while (fgets(task,SIZE,in) != NULL) {
    ++num_addresses;

    uint32_t logical_address = (uint32_t)(atoi(task));
    uint16_t sixteen_bit_logical_address = (uint16_t)(logical_address & 0xFFFF);

    AddressUnion user_address;
    user_address.ul = sixteen_bit_logical_address;

    translateAddress(sixteen_bit_logical_address, user_address.bitfield.page_num, user_address.bitfield.page_offset);
  }

  fclose(in);

  printf("\nPage-fault rate: %f%%\n", ((float)num_page_faults/num_addresses)*100);
  printf("TLB hit rate: %f%%\n", ((float)num_TLB_hits/num_addresses)*100);

  return 0;
}

void translateAddress(int address, int pageNumber, int offset)
{
  printf("\nLogical address being translated: %d\n", address);
  if (strcmp(consultTLB(pageNumber, offset), "TLB hit") == 0)
  {
    return;
  }
  consultPageTable(pageNumber, offset);

  return;
}

char* consultTLB(int pageNumber, int offset)
{
  int i = 0;
  char* TLB_status = "TLB miss";
  for (i = 0; i < 16; i++)
  {
    if ((TLB[i].page_number == pageNumber && (TLB[i].altered == 'a')))
    {
      /* TLB hit */
      TLB_status = "TLB hit";
      int physical_address = (256*TLB[i].frame_number) + offset;
      printf("Corresponding physical address: %d\n", physical_address);
      printf("Signed byte value at this physical address: \'%d\'\n", (signed char)physical_memory[TLB[i].frame_number].bytes[offset]);
   
      ++num_TLB_hits;
      /* increment each TLB entry's LRU number */
      for (i = 0; i < 16; i++)
      {
        if (TLB[i].altered == 'a')
        {
          TLB[i].LRU_number += 1;
        }
      }
      return TLB_status;
    }
  }  
  return TLB_status;
}

char* consultPageTable(int pageNumber, int offset)
{
  if ((PageTable[pageNumber].page_number == pageNumber) && (PageTable[pageNumber].valid_invalid_bit == 'v'))
  {
    int physical_address = (256*PageTable[pageNumber].frame_number) + offset;
    printf("Corresponding physical address: %d\n", physical_address);
    printf("Signed byte value at this physical address: \'%d\'\n", (signed char)physical_memory[PageTable[pageNumber].frame_number].bytes[offset]);
  
    /* get this page into TLB */
    int i = 0;
    TLBEntry t;
    t.frame_number = PageTable[pageNumber].frame_number;
    t.page_number = pageNumber;
    t.altered = 'a';
    t.LRU_number = 0;

    if (num_TLB_entries != 16)
    {
      ++num_TLB_entries;
      for (i = 0; i < 16; i++)
      {
        if (TLB[i].altered == 'u')
        {
          TLB[i] = t;
          break;
        }
      } 
    }
    else
    {
      /* implement Least Recently Used replacement algorithm */ 
      int current_largest_LRU_number = 0;
      int current_largest_LRU_number_index = 0;
      for (i = 0; i < 16; i++)
      {
        if (TLB[i].LRU_number > current_largest_LRU_number)
        {
          current_largest_LRU_number = TLB[i].LRU_number;
          current_largest_LRU_number_index = i;
        }
      }
      TLB[current_largest_LRU_number_index] = t;
    }
    /* increment each TLB entry's LRU number */
    for (i = 0; i < 16; i++)
    {
      if (TLB[i].altered == 'a')
      {
        TLB[i].LRU_number += 1;
      }
    }
    return "Page Table Hit";
  }
  /* read in a 256-byte page from the file BACKING STORE and 
  store it in an available page frame in physical memory */
  /*  For example, if a logical address with page number 15 resulted in a page fault, your
  program would read in page 15 from BACKING STORE and store it in physical memory */
  ++num_page_faults;

  FILE* file_pointer;
  file_pointer = fopen("BACKING_STORE.bin", "r");

  if (file_pointer == NULL)
  {
    printf("Could not open BACKING_STORE.bin\n");
    exit(1);
  }
  int i = 0;
  int free_page_number = 0;
  for (i = 0; i < 256; i++)
  {
    if (freePageList[i].is_free == 'f')
    {
      free_page_number = i;
      freePageList[i].is_free = 'n';
      break;
    }
  }
  fseek(file_pointer, pageNumber*256, SEEK_SET);
  fread(physical_memory[free_page_number].bytes, 1, 256, file_pointer);
  fclose(file_pointer);

  int physical_address = (256*free_page_number) + offset;
  printf("Corresponding physical Address: %d\n", physical_address);
  printf("Signed byte value at this physical address: \'%d\'\n", (signed char)physical_memory[free_page_number].bytes[offset]);
 

  PageTable[pageNumber].valid_invalid_bit = 'v';
  PageTable[pageNumber].frame_number = free_page_number;
  PageTable[pageNumber].page_number = pageNumber;

  /* get this page into TLB */
  TLBEntry t;
  t.frame_number = free_page_number;
  t.page_number = pageNumber;
  t.altered = 'a';
  t.LRU_number = 0;

  if (num_TLB_entries != 16)
  {
    ++num_TLB_entries;
    for (i = 0; i < 16; i++)
    {
      if (TLB[i].altered == 'u')
      {
        TLB[i] = t;
        break;
      }
    } 
  }
  else
  {
    /* implement Least Recently Used replacement algorithm */ 
    int current_largest_LRU_number = 0;
    int current_largest_LRU_number_index = 0;
    for (i = 0; i < 16; i++)
    {
      if (TLB[i].LRU_number > current_largest_LRU_number)
      {
        current_largest_LRU_number = TLB[i].LRU_number;
        current_largest_LRU_number_index = i;
      }
    }
    TLB[current_largest_LRU_number_index] = t;
  }
  /* increment each TLB entry's LRU number */
  for (i = 0; i < 16; i++)
  {
    if (TLB[i].altered == 'a')
    {
      TLB[i].LRU_number += 1;
    }
  }

  return "Page Fault";
}