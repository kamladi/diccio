#include <stdio.h>
#include <stdint.h>
#include <stdlib.h> 

#define DEBUG 0 
#define MAX_BUF_SIZE 20000
#define MAX_EDGES 50

/* edgeDetect
 * 
 * Takes signal, standard deviation and minimum step change and
 * outputs the sample number where edge was detected 
 */
uint16_t edgeDetect(uint32_t* signal, uint16_t signal_size, uint16_t pre_win_size, uint32_t stdev, uint32_t v_min);

int main(int argc, char* argv[])
{
  FILE *fp;
  uint16_t i=0;
  uint8_t red=0;
  uint8_t batches=1; /* Number of batches in which to process the data */
  uint16_t edge[MAX_EDGES]; /* Detected edges */
  uint32_t stdev = 10; 	/* Standard deviation */
  uint32_t v_min = 30;	/* Minimum change in mean */
  uint16_t pre_win_size = 30;	/* Size of pre-event averaging window */
  uint16_t batch_window_size = 0;	/* Size of the window that will be used to split the signal when batch processing */
  
  uint32_t signal[MAX_BUF_SIZE]; /* The signal to be processed */
  uint16_t signal_size = 0,ts;


  // 1 400 30 50
  if (argc < 2) {
    printf("\nNo input file given. \n\n");
    printf("Usage: %s input.csv stdev v_min pre_win_size batch_window_size\n\n", argv[0]);
    printf("  input.csv = \t\t\t\tInput filename. Expects CSV format.\n");
    printf("  stdev [optional] = \t\t\tStandard deviation of the power signal.\n");
    printf("  v_min [optional] = \t\t\tMinimum power change of interest (in same units as original data).\n");
    printf("  pre_win_size [optional] = \t\tSize of the window for calculating pre-event mean values.\n");
    printf("  batch_window_size [optional] = \tSize of the window that will be used to process the data in batches.\n\n");
    exit(0);
  }
  else {
    printf("input file: %s\n", argv[1]);
    
    /* open the file */
    fp = fopen(argv[1], "r");
    if (fp == NULL) {
      printf("Could not open %s\n", argv[1]);
      exit(0);
    }

    /* read it and put it in signal */
    do {
       red = fscanf(fp, "%lu, %lu\n", &ts,signal+i);
       i++;
    } while (red == 2 && i < MAX_BUF_SIZE);
    
    /* update window sizes */
    signal_size = i-1;
    batch_window_size = signal_size;
    /* check if other arguments were given */
    if (argc >=3)
      stdev = atoi(argv[2]);
    if (argc >=4)
      v_min = atoi(argv[3]);
    if (argc >=5)
      pre_win_size = atoi(argv[4]);
    if (argc >=6) {
      batch_window_size = atoi(argv[5]); 
      batches = (signal_size / batch_window_size);
      if (batches > MAX_EDGES) {
	printf("Number of batch processes exceeds the allowed limit (%u)\n", MAX_EDGES);
	exit(0);
      }
    }
  }
  
  printf("Processing data in %d batch(es)...\n", batches);
  red = 0; /* red will be a counter here */
  for (i=0; i<batches; i++) {
    printf("Batch #%d started. ", i);
    
    /* Process the batch */
    edge[red] = edgeDetect(&(signal[i*batch_window_size]), batch_window_size, pre_win_size, stdev, v_min);
    
    printf("Completed.\n");
    if (edge[red] > 0) {
      /* Correct the index */
      edge[red] += i*batch_window_size;
      /* increase the counter */
      red++;
    }
  }

  if(DEBUG) {
    for (i=0; i<signal_size-1; i++)
      printf("[%d] %d, ", i, signal[i]);
    printf("[%d] %d\n", signal_size-1, signal[signal_size-1]);
  }

  printf("\n\n");
  
  /* If any edges were found... */ 
  if(red) {
    printf("Found edges at: "); 
    for (i=0; i<red; i++)
      printf("%d, ", edge[i]);
    printf("\n\n");
  } else {
    printf("Did not find any edges.\n\n");
  }
  
  return 0;

}

uint16_t edgeDetect(uint32_t* signal, uint16_t k, uint16_t pre_win_size, uint32_t stdev, uint32_t v_min) {
  uint16_t i, j, edge;
  int32_t V, test; /* V = u1 - u0   i.e. the difference between mean values in post and pre event windows */
  int32_t sum = 0; /* temporary helper variable */
  int32_t max_j = 0; /* The maximum value of of the decision statistic, updated every iteration */
  int32_t u0 = 0; /* Pre-event mean */
  
  edge = 0;

  for (j=pre_win_size; j<k; j++) {
    sum = 0;
    u0 = 0;
   
    /* Compute the pre-event averaging window */
    for (i=j-pre_win_size; i<=j; i++) {
      u0 += signal[i]; 
    }
    u0 = u0 / pre_win_size;
    
    /* Find the right Vj */
    for (i=j; i<k; i++) {
      sum += abs(signal[i] - u0);
    }
    test = 1/(k-j+1) * sum;
    V = test < v_min ? v_min : test;

    sum = 0;

    /* Compute the sum and check if it's greater than previous values */
    for (i=j; i<k; i++) {
      sum += V*(signal[i] - u0) / (stdev*stdev) - V*V / (2*stdev*stdev);
    } 
    if (sum > max_j) {
      edge = j;
      max_j = sum;
    } 
  }
  
  return edge == 0 ? 0 : edge+1;
}

