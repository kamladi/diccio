/**
 * 18-748 Wireless Sensor Networks
 * Spring 2016
 * Dicio - A Smart Outlet Mesh Network
 * main.c (gateway)
 * Kedar Amladi // kamladi. Daniel Santoro // ddsantor. Adam Selevan // aselevan.
 */

/***** PREABLE *****/

// INCLUDES
// standard nrk
#include <nrk.h>
#include <nrk_events.h>
#include <include.h>
#include <ulib.h>
#include <stdio.h>
#include <stdlib.h>
#include <avr/sleep.h>
#include <hal.h>
#include <bmac.h>
#include <nrk_error.h>
#include <nrk_sw_wdt.h>
// this package
#include <assembler.h>
#include <packet_queue.h>
#include <parser.h>
#include <pool.h>
#include <type_defs.h>

// DEFINES
#define MAC_ADDR 1

// FUNCTION DECLARATIONS
uint8_t inline atomic_size(packet_queue *pq, nrk_sem_t *mux);
void inline atomic_push(packet_queue *pq, packet *p, nrk_sem_t *mux);
void inline atomic_pop(packet_queue *pq, packet *p, nrk_sem_t *mux);
uint16_t inline atomic_increment_seq_num();
uint8_t inline atomic_received_ack();
void inline atomic_update_received_ack(uint8_t update);
void tx_cmds(void);
void tx_node(void);
uint8_t get_server_input(void);
void copy_packet(packet *dest, packet *src);
void clear_serv_buf();
void rx_node_task(void);
void rx_serv_task(void);
void tx_net_task(void);
void tx_serv_task(void);
void hand_task(void);
void nrk_create_taskset ();

// TASKS
nrk_task_type RX_NODE_TASK;
nrk_task_type RX_SERV_TASK;
nrk_task_type TX_NET_TASK;
nrk_task_type TX_SERV_TASK;
nrk_task_type HAND_TASK;
nrk_task_type ALIVE_TASK;

// TASK STACKS
NRK_STK rx_node_task_stack[NRK_APP_STACKSIZE];
NRK_STK rx_serv_task_stack[NRK_APP_STACKSIZE];
NRK_STK tx_net_task_stack[NRK_APP_STACKSIZE*8];
NRK_STK tx_serv_task_stack[NRK_APP_STACKSIZE];
NRK_STK hand_task_stack[NRK_APP_STACKSIZE];
NRK_STK alive_task_stack[NRK_APP_STACKSIZE];

// BUFFERS
uint8_t g_net_rx_buf[RF_MAX_PAYLOAD_SIZE];
uint8_t g_serv_rx_buf[MAX_RX_UART_BUF];
uint8_t g_serv_rx_index = 0;
uint8_t g_serv_tx_index = 0;
uint8_t g_net_tx_buf[RF_MAX_PAYLOAD_SIZE];
uint8_t g_net_tx_index = 0;
nrk_sem_t* g_net_tx_buf_mux;
uint8_t g_serv_tx_buf[RF_MAX_PAYLOAD_SIZE];

// QUEUES
packet_queue g_cmd_tx_queue;
nrk_sem_t* g_cmd_tx_queue_mux;
packet_queue g_node_tx_queue;
nrk_sem_t* g_node_tx_queue_mux;
packet_queue g_serv_tx_queue;
nrk_sem_t* g_serv_tx_queue_mux;
packet_queue g_hand_rx_queue;
nrk_sem_t* g_hand_rx_queue_mux;

// DRIVERS
void nrk_register_drivers();

// SEQUENCE POOLS/NUMBER
pool_t g_seq_pool;
uint16_t g_seq_num = 0;
nrk_sem_t* g_seq_num_mux;
uint16_t g_cmd_id = 0;

// ALIVE POOL
pool_t g_alive_pool;
nrk_sem_t* g_alive_pool_mux;

// COMMAND FLAGS
uint8_t g_cmd_ack_received = TRUE;
nrk_sem_t * g_cmd_mux;

// GLOBAL FLAG
uint8_t g_verbose;

// Global last cmd
packet g_last_cmd;
uint8_t g_retry_cmd_counter = 0;
uint8_t g_retry_limit_counter = 0;

/***** END PREABMLE *****/

/***** MAIN *****/

int main () {
  // setup ports/uart
  nrk_setup_ports ();
  nrk_setup_uart (UART_BAUDRATE_38K4);
  nrk_init ();

  // clear all LEDs
  nrk_led_clr(0);
  nrk_led_clr(1);
  nrk_led_clr(2);
  nrk_led_clr(3);

  // print flag
  g_verbose = FALSE;

  // mutexs
  g_net_tx_buf_mux    = nrk_sem_create(1, 9);
  g_cmd_tx_queue_mux  = nrk_sem_create(1, 9);
  g_node_tx_queue_mux = nrk_sem_create(1, 9);
  g_serv_tx_queue_mux = nrk_sem_create(1, 9);
  g_hand_rx_queue_mux = nrk_sem_create(1, 9);
  g_seq_num_mux       = nrk_sem_create(1, 9);
  g_alive_pool_mux    = nrk_sem_create(1, 9);
  g_cmd_mux           = nrk_sem_create(1, 9);

  // packet queues
  packet_queue_init(&g_cmd_tx_queue);
  packet_queue_init(&g_node_tx_queue);
  packet_queue_init(&g_serv_tx_queue);
  packet_queue_init(&g_hand_rx_queue);

  nrk_time_set (0, 0);
  bmac_task_config();
  nrk_create_taskset();
  bmac_init (13);
  nrk_start ();
  return 0;
}

/***** END MAIN *****/

/***** HELPER FUNCTIONS *****/
uint8_t inline atomic_size(packet_queue *pq, nrk_sem_t *mux) {
  uint8_t toReturn;
  nrk_sem_pend(mux); {
    toReturn = pq->size;
  }
  nrk_sem_post(mux);
  return toReturn;
}

// atomic_push - push onto the queue atomically
void inline atomic_push(packet_queue *pq, packet *p, nrk_sem_t *mux) {
  nrk_sem_pend(mux); {
    push(pq, p);
  }
  nrk_sem_post(mux);
}

// atomic_pop - pop onto the queue atomically
void inline atomic_pop(packet_queue *pq, packet *p, nrk_sem_t *mux) {
  nrk_sem_pend(mux); {
    pop(pq, p);
  }
  nrk_sem_post(mux);
}

uint8_t inline atomic_received_ack(){
  uint8_t returnVal;
  nrk_sem_pend(g_cmd_mux); {
    returnVal = g_cmd_ack_received;
  }
  nrk_sem_post(g_cmd_mux);

  return returnVal;
}

void inline atomic_update_received_ack(uint8_t update){
  nrk_sem_pend(g_cmd_mux); {
    g_cmd_ack_received = update;
  }
  nrk_sem_post(g_cmd_mux);
}

// atomic_increment_seq_num - increment sequence number atomically and return
uint16_t inline atomic_increment_seq_num() {
  uint16_t returnVal;
  nrk_sem_pend(g_seq_num_mux); {
    g_seq_num++;
    returnVal = g_seq_num;
  }
  nrk_sem_post(g_seq_num_mux);
  return returnVal;
}

void copy_packet(packet *dest, packet *src){
  dest->type = src->type;
  dest->source_id = src->source_id;
  dest->seq_num = src->seq_num;
  dest->num_hops = src->num_hops;

  for(uint8_t i = 0; i < MAX_PACKET_BUFFER; i++){
    dest->payload[i] = src->payload[i];
  }
}

// get_server_input() - get UART data from server - end of message noted by a '\r'
uint8_t get_server_input() {
  uint8_t received;

  // loop until all bytes have been received
  while(nrk_uart_data_ready(NRK_DEFAULT_UART)) {

    // get UART byte and add to buffer
    received = getchar();

    // if there is room, add it to the buffer.
    if((RF_MAX_PAYLOAD_SIZE -1) > g_serv_rx_index) {
      g_serv_rx_buf[g_serv_rx_index] = received;
      g_serv_rx_index++;
    }
    // if there is not room, clear the buffer and then add the new byte.
    else {
      clear_serv_buf();
      g_serv_rx_buf[g_serv_rx_index] = received;
      g_serv_rx_index++;
    }

    // print if appropriate
    if(TRUE == g_verbose) {
      printf("!%d", received);
    }

    // message has been completed
    if('\r' == received) {
      g_serv_rx_buf[g_serv_rx_index] = '\n';
      g_serv_rx_index++;
      if(TRUE == g_verbose) {
        nrk_kprintf(PSTR("\n"));
      }
      return SERV_MSG_RECEIVED;
    }
  }
  return SERV_MSG_INCOMPLETE;
}

// clear_serv_buf - clear the server recieve buffer
void clear_serv_buf() {
  for(uint8_t i = 0; i < g_serv_rx_index; i++) {
    g_serv_rx_buf[i] = 0;
  }
  g_serv_rx_index = 0;
}

void clear_serv_tx_buf(){
  for(uint8_t i = 0; i < RF_MAX_PAYLOAD_SIZE; i++){
    g_serv_tx_buf[i] = 0;
  }
}

// clear_tx_buf - clear the network transmit buffer
void clear_tx_buf(){
  for(uint8_t i = 0; i < g_net_tx_index; i++){
    g_net_tx_buf[i] = 0;
  }
  g_net_tx_index = 0;
}

/***** END HELPER FUNCTIONS *****/

/***** TASKS *****/

// rx_node_task - receive messages from the network
void rx_node_task() {
  // local variable instantiation
  packet rx_packet;
  uint8_t len;
  int8_t rssi;
  int8_t in_seq_pool;
  int8_t in_alive_pool;
  uint16_t local_seq_num;
  uint8_t new_node = NONE;
  uint8_t rx_source_id;
  uint16_t rx_seq_num;
  msg_type rx_type;

  printf("rx_node_task PID: %d.\r\n", nrk_get_pid());

  // initialize network receive buffer
  bmac_rx_pkt_set_buffer(g_net_rx_buf, RF_MAX_PAYLOAD_SIZE);

  // Wait until bmac has started. This should be called by all tasks using bmac that do not call bmac_init()
  while (!bmac_started ()){
    nrk_wait_until_next_period ();
  }

  // loop forever
  while(1) {
    // only execute if there is a packet available
    if(bmac_rx_pkt_ready()) {
      nrk_led_set(GREEN_LED);

      bmac_rx_pkt_get(&len, &rssi);
      // get the packet, parse and release
      parse_msg(&rx_packet, (uint8_t *)&g_net_rx_buf, len);

      bmac_rx_pkt_release ();

      // print incoming packet if appropriate
      if(TRUE == g_verbose) {
        nrk_kprintf(PSTR("RX network: "));
        print_packet(&rx_packet);
      }

      // only receive the message if it's not from the gateway
      //  NOTE: this is required because the gateway will hear re-transmitted packets
      //    originally from itself.
      rx_source_id = rx_packet.source_id;
      if(MAC_ADDR != rx_source_id) {

        // check to see if this node is in the sequence pool, if not then add it
        in_seq_pool = in_pool(&g_seq_pool, rx_packet.source_id);
        if(NOT_IN_POOL == in_seq_pool) {
          add_to_pool(&g_seq_pool, rx_packet.source_id, rx_packet.seq_num);
          new_node = NODE_FOUND;
        }

        // determine if we should act on this packet based on the sequence number
        local_seq_num = get_data_val(&g_seq_pool, rx_packet.source_id);
        rx_seq_num = rx_packet.seq_num;
        rx_type = rx_packet.type;
        if((rx_seq_num > local_seq_num) || (NODE_FOUND == new_node) || (MSG_HAND == rx_type)) {

          // check to see if this node is in the ALIVE pool, if not then add it,
          // If it is in the alive pool, update the counter to HEART FACTOR
          nrk_sem_pend(g_alive_pool_mux);{
            in_alive_pool = in_pool(&g_alive_pool, rx_packet.source_id);
            if(NOT_IN_POOL == in_alive_pool) {
              add_to_pool(&g_alive_pool, rx_packet.source_id, HEART_FACTOR);
            } else {
              update_pool(&g_alive_pool, rx_packet.source_id, HEART_FACTOR);
            }
          }
          nrk_sem_post(g_alive_pool_mux);

          // update the sequence pool and reset the new_node flag
          update_pool(&g_seq_pool, rx_packet.source_id, rx_packet.seq_num);
          new_node = NONE;

          // put the message in the right queue based on the type
          switch(rx_type) {
            // command -> do nothing
            case MSG_CMD: {
              // do nothing...commands are sent by the gateway, so there is no
              //  no need to pass it along again.
              break;
            }
            // command ack -> forward to server
            case MSG_CMDACK: {
              // set CMD_ACK_RECEIVED flag to true
              atomic_update_received_ack(TRUE);

              rx_packet.num_hops++;
              atomic_push(&g_serv_tx_queue, &rx_packet, g_serv_tx_queue_mux);
              break;
            }
            // data received  -> forward to server
            case MSG_DATA: {
              atomic_push(&g_serv_tx_queue, &rx_packet, g_serv_tx_queue_mux);
              break;
            }
            // handshake message recieved -> deal with in handshake function
            case MSG_HAND: {
              atomic_push(&g_hand_rx_queue, &rx_packet, g_hand_rx_queue_mux);
              break;
            }
            case MSG_HANDACK: {
              // received an ACK. Do nothing?
              break;
            }
            // gateway message -> do nothing
            case MSG_GATEWAY: { 
              // do nothing...no messages have been defined with this type yet
              break;
            }
            case MSG_HEARTBEAT: { 
              // do nothing...heartbeats are sent by the gateway, so there is no
              //  no need to pass it along.              
              break;
            }
            // no message
            case MSG_NO_MESSAGE: {
              // do nothing.
              // NOTE: this is a valid case. If the message is not 'parsible' then it can be
              //  given a 'NO_MESSAGE' type.
              break;
            }
            default: {
              // do nothing
              // NOTICE: really this should never happen. Eventually, throw and error here.
              break;
            }
          }
        }
      }
      nrk_led_clr(GREEN_LED);
    }
    nrk_wait_until_next_period();
  }
  nrk_kprintf (PSTR ("out rx_node\r\n"));
}

// rx_serv_task - receive messages from the server
void rx_serv_task() {
  // local variable instantiation
  packet rx_packet;
  uint16_t server_seq_num = 0;
  msg_type rx_type;

  printf("rx_serv_task PID: %d.\r\n", nrk_get_pid());

  // get the UART signal and register it
  nrk_sig_t uart_rx_signal = nrk_uart_rx_signal_get();
  nrk_signal_register(uart_rx_signal);

  // loop forever
  while (1) {
    // only execute if a full server message has been received
    if(SERV_MSG_RECEIVED == get_server_input()) {
      nrk_led_set(BLUE_LED);

      // parse message
      parse_msg(&rx_packet, (uint8_t *)&g_serv_rx_buf, g_serv_rx_index);
      clear_serv_buf();
      if(TRUE == g_verbose) {
        nrk_kprintf (PSTR ("RX Server: "));
        print_packet(&rx_packet);
      }

      // update server sequence number
      rx_packet.seq_num = server_seq_num;
      server_seq_num++;
      rx_packet.num_hops++;
      rx_type = rx_packet.type;
      switch(rx_type) {
        // command received
        case MSG_CMD: {
          atomic_push(&g_cmd_tx_queue, &rx_packet, g_cmd_tx_queue_mux);
          break;
        }
        case MSG_CMDACK:
        case MSG_DATA:
        case MSG_HAND:
        case MSG_GATEWAY:
        case MSG_HEARTBEAT:
         // do nothing
          // NOTICE: really, these should never happend. Eventually, throw an error here.
          break;
        case MSG_NO_MESSAGE: {
          // do nothing.
          // NOTE: this is a valid case. If the message is not 'parsible' then it can be
          //  given a 'NO_MESSAGE' type.
          break;
        }
        default: {
          // do nothing
          // NOTICE: really this should never happen. Eventually, throw an error here.
          break;
        }
      }
      nrk_led_clr(BLUE_LED);
    }
    nrk_wait_until_next_period();
  }
  nrk_kprintf (PSTR ("out rx_serv\r\n"));
}

// tx_cmd_task - send all commands out to the network
void tx_cmds() {
  // local variable instantiation
  uint16_t val;
  nrk_sig_t tx_done_signal;
  packet tx_packet;
  uint8_t local_tx_cmd_queue_size;
  uint8_t local_cmd_ack_received = FALSE;

  // Wait until bmac has started. This should be called by all tasks
  //  using bmac that do not call bmac_init().
  while(!bmac_started()) {
    nrk_wait_until_next_period();
  }
  
  local_cmd_ack_received = atomic_received_ack();

  // If we have received an ack
  if(TRUE == local_cmd_ack_received){
    // atomically get the queue size
    local_tx_cmd_queue_size = atomic_size(&g_cmd_tx_queue, g_cmd_tx_queue_mux);

    // If there is a command to send.
    if(0 < local_tx_cmd_queue_size){
      nrk_led_set(RED_LED);

      // get a packet out of the queue.
      atomic_pop(&g_cmd_tx_queue, &tx_packet, g_cmd_tx_queue_mux);

      // increment the sequence number and copy packet
      tx_packet.seq_num = atomic_increment_seq_num();
      copy_packet(&g_last_cmd, &tx_packet);

      // transmit the command
      nrk_sem_pend(g_net_tx_buf_mux); {
        g_net_tx_index = assemble_packet((uint8_t *)&g_net_tx_buf, &tx_packet);

        val = bmac_tx_pkt(g_net_tx_buf, g_net_tx_index);
        if(NRK_OK != val){
          nrk_kprintf( PSTR( "NO ack or Reserve Violated!\r\n" ));
        }
      }
      nrk_sem_post(g_net_tx_buf_mux);

      // sent a command, need an ack
      atomic_update_received_ack(FALSE);
      g_retry_limit_counter = 0;

      nrk_led_clr(RED_LED);
    }
  }
  // have not received an ack...
  else{
    g_retry_cmd_counter ++;
    if(RETRY_CMD_PERIOD <= g_retry_cmd_counter){
      if(RETRY_LIMIT <= g_retry_limit_counter){
        // never received ack.. move on to the next
        g_retry_limit_counter = 0;
        atomic_update_received_ack(TRUE);
      }
      else{
        g_retry_limit_counter ++;
        // increment the sequence number
        g_last_cmd.seq_num = atomic_increment_seq_num();       

       // print if appropriate
        if (TRUE == g_verbose) {
          nrk_kprintf (PSTR ("RETRY PACKET:"));
          print_packet(&g_last_cmd);
        }

        // reset counter and sent
        g_retry_cmd_counter = 0;
        nrk_sem_pend(g_net_tx_buf_mux); {
          g_net_tx_index = assemble_packet((uint8_t *)&g_net_tx_buf, &g_last_cmd);

          val = bmac_tx_pkt(g_net_tx_buf, g_net_tx_index);
          if(NRK_OK != val){
            nrk_kprintf( PSTR( "NO ack or Reserve Violated!\r\n" ));
          }
        }
        nrk_sem_post(g_net_tx_buf_mux);
      }
    }
  }
}

// tx_serv_task - transmit message to the server
void tx_serv_task() {
  uint8_t local_tx_serv_queue_size;
  packet tx_packet;

  printf("tx_serv_task PID: %d.\r\n", nrk_get_pid());
  // loop forever
  while(1) {
    // atomically get the queue size
    local_tx_serv_queue_size = atomic_size(&g_serv_tx_queue, g_serv_tx_queue_mux);

    // loop on queue size received above, and no more.
    for(uint8_t i = 0; i < local_tx_serv_queue_size; i++) {
      // get a packet out of the queue, assemble and send
      atomic_pop(&g_serv_tx_queue, &tx_packet, g_serv_tx_queue_mux);
      assemble_serv_packet((uint8_t *)&g_serv_tx_buf, &tx_packet);
      printf("%s\r\n", g_serv_tx_buf);
    }
    clear_serv_tx_buf();
    nrk_wait_until_next_period();
  }
  nrk_kprintf (PSTR ("out tx_serv\r\n"));
}

// tx_node_task - send standard messages out to the network (i.e. heartbeat messages, etc.)
void tx_node() {
  // local variable initialization
  uint16_t val;
  packet tx_packet;
  uint8_t local_tx_node_queue_size;
  uint8_t sent_handack = FALSE;
  uint8_t to_send;
  msg_type tx_type;

  // Wait until bmac has started. This should be called by all tasks
  //  using bmac that do not call bmac_init().
  while(!bmac_started ()) {
    nrk_wait_until_next_period ();
  }

  // atomically get the queue size
  local_tx_node_queue_size = atomic_size(&g_node_tx_queue, g_node_tx_queue_mux);

  // loop on queue size received above, and no more.
  for(uint8_t i = 0; i < local_tx_node_queue_size; i++) {
    nrk_led_set(RED_LED);

    // get a packet out of the queue.
    atomic_pop(&g_node_tx_queue, &tx_packet, g_node_tx_queue_mux);
    tx_type = tx_packet.type;
    if((MSG_HANDACK == tx_type) && (TRUE == sent_handack)) {
      to_send = FALSE;
    } else {
      to_send = TRUE;
    }

    if(TRUE == to_send){
      // transmit to nodes
      nrk_sem_pend(g_net_tx_buf_mux); {
        g_net_tx_index = assemble_packet((uint8_t *)&g_net_tx_buf, &tx_packet);

        if(TRUE == g_verbose) {
          nrk_kprintf (PSTR ("TX Node: "));
          print_packet(&tx_packet);
        }
        
        // send the packet
        val = bmac_tx_pkt(g_net_tx_buf, g_net_tx_index);
        if(NRK_OK != val){
          nrk_kprintf( PSTR( "NO ack or Reserve Violated!\r\n" ));
        }

        // set flag
        if(MSG_HANDACK == tx_type){
          sent_handack = TRUE;
        }

        clear_tx_buf();
      }
      nrk_sem_post(g_net_tx_buf_mux);
    }
    nrk_led_clr(RED_LED);
  }
}

// net_tx_task - send network messages
void tx_net_task() {
  uint8_t counter = 0;
  uint8_t tx_cmd_flag;
  uint8_t tx_data_flag;

  printf("tx_net PID: %d.\r\n", nrk_get_pid());

  // loop forever
  while(1) {
    // incrment counter and set flags
    counter++;
    tx_cmd_flag = counter % TX_CMD_FLAG;
    tx_data_flag = counter % GATE_TX_DATA_FLAG;

    // if commands should be transmitted, then call the tx_cmds() helper
    if (TRANSMIT == tx_cmd_flag) {
      tx_cmds();
    }

    // if data shoudl be transmitted, then call the tx_data() helper
    if (TRANSMIT == tx_data_flag) {
      tx_node();
      counter = 0;
    }
    nrk_wait_until_next_period();
  }
  nrk_kprintf (PSTR ("out tx_net \r\n"));
}

// alive_task 
//  - send heartbeat message to the network and user (LEDS) 
//  - check heartbeat status of all nodes in the network
void alive_task() {
  uint8_t LED_FLAG = 0;
  packet heart_packet, lost_packet;
  uint8_t temp_id;
  uint8_t local_alive_pool_size;
  uint8_t gateway_reset_counter = 0;

  printf("alive_task PID: %d.\r\n", nrk_get_pid());

  // initialize lost_packet
  lost_packet.source_id = MAC_ADDR;
  lost_packet.type = MSG_LOST;
  lost_packet.num_hops = 0;

  // initialize heart_packet
  heart_packet.source_id = MAC_ADDR;
  heart_packet.type = MSG_HEARTBEAT;
  heart_packet.num_hops = 0;

  // loop forever
  while(1) {
    //LED functionality gives visible indication of functionality of the gateway
    LED_FLAG += 1;
    LED_FLAG %= 2;
    if(0 == LED_FLAG) {
      nrk_led_set(GREEN_LED);
    }
    else {
      nrk_led_clr(GREEN_LED);
    }

    // increment the sequence number
    heart_packet.seq_num = atomic_increment_seq_num();

    // if we haven't send reset more than MAX_RESET_SENDS
    if(MAX_RESET_SENDS > gateway_reset_counter){
      heart_packet.type = MSG_RESET;
      gateway_reset_counter ++;
    }
    else{
      heart_packet.type = MSG_HEARTBEAT;
    }
    // add to the g_node_tx_queue -> send out on network
    atomic_push(&g_node_tx_queue, &heart_packet, g_node_tx_queue_mux);

    // add to the g_serv_tx_queue -> send to the server
    atomic_push(&g_serv_tx_queue, &heart_packet, g_serv_tx_queue_mux);

    // decrement all items in alive pool
    nrk_sem_pend(g_alive_pool_mux);{
      decrement_all(&g_alive_pool);
      local_alive_pool_size = g_alive_pool.size;
    }
    nrk_sem_post(g_alive_pool_mux);

    // check all items in the alive pool to determine if any 
    //  counters have expired
    for(uint8_t i = 0; i < local_alive_pool_size; i ++){
      // set temp_id to an invalid id
      temp_id = 0;

      // if alive_pool[i] is NOT_ALIVE set temp_id flags
      nrk_sem_pend(g_alive_pool_mux);{
        uint8_t temp_val = g_alive_pool.data_vals[i];
        if(ALIVE_LIMIT == temp_val){
          g_alive_pool.data_vals[i] = NOT_ALIVE;
          temp_id = g_alive_pool.node_id[i];
        }
      }
      nrk_sem_post(g_alive_pool_mux);

      // if alive_pool[i] is NOT_ALIVE - send message to the server
      if(0 != temp_id){
        lost_packet.payload[LOST_NODE_INDEX] = temp_id;
        atomic_push(&g_serv_tx_queue, &lost_packet, g_serv_tx_queue_mux);

      }
    }

    // wait until next period to send again
    nrk_wait_until_next_period();
  }
  nrk_kprintf (PSTR ("out alive_task\r\n"));
}

// hand_task - handle handshakes
void hand_task() {
  uint8_t local_hand_rx_queue_size;
  packet rx_packet, tx_packet;
  int8_t in_ack_pool;
  pool_t ack_pool;
  
  printf("hand_task PID: %d.\r\n", nrk_get_pid());

  // initialize HANDACK packet
  tx_packet.source_id = MAC_ADDR;
  tx_packet.type = MSG_HANDACK;
  tx_packet.num_hops = 0;

  // loop forever
  while(1) {
    // every iteration of this task will yield a new pool
    clear_pool(&ack_pool);

    // atomically get queue size
    local_hand_rx_queue_size = atomic_size(&g_hand_rx_queue, g_hand_rx_queue_mux);

    // loop on queue size received above, and no more.
    for(uint8_t i = 0; i < local_hand_rx_queue_size; i++) {
      // get a packet out of the queue.
      atomic_pop(&g_hand_rx_queue, &rx_packet, g_hand_rx_queue_mux);

      // determine if this node has been seen during this iteration
      in_ack_pool = in_pool(&ack_pool, rx_packet.source_id);

      // if the node has not been seen yet this iteration, then send a HANDACK
      if(NOT_IN_POOL == in_ack_pool) {
        add_to_pool(&g_seq_pool, rx_packet.source_id, rx_packet.seq_num);
        // increment sequence number atomically


        tx_packet.seq_num = atomic_increment_seq_num();

        // finish transmit packet
        tx_packet.payload[HANDACK_NODE_ID_INDEX] = rx_packet.source_id;
        tx_packet.payload[HANDACK_CONFIG_ID_INDEX] = rx_packet.payload[HAND_CONFIG_ID_INDEX];
        tx_packet.payload[HANDACK_CONFIG_ID_INDEX + 1] = rx_packet.payload[HAND_CONFIG_ID_INDEX +1];
        tx_packet.payload[HANDACK_CONFIG_ID_INDEX + 2] = rx_packet.payload[HAND_CONFIG_ID_INDEX +2];
        tx_packet.payload[HANDACK_CONFIG_ID_INDEX + 3] = rx_packet.payload[HAND_CONFIG_ID_INDEX +3];

        // send response back to the node and to the server
        atomic_push(&g_node_tx_queue, &tx_packet, g_node_tx_queue_mux);
        atomic_push(&g_serv_tx_queue, &tx_packet, g_serv_tx_queue_mux);
      }
    }
    nrk_wait_until_next_period();
  }
  nrk_kprintf (PSTR ("out hand_task\r\n"));
}

/***** END TASKS ****/

/***** CONFIGURATION *****/
/**
 * nrk_create_taskset - create the tasks in this application
 *
 * NOTE: task priority maps to importance. That is, priority(5) > priority(2).
 */
void nrk_create_taskset () {
  RX_NODE_TASK.task = rx_node_task;
  nrk_task_set_stk(&RX_NODE_TASK, rx_node_task_stack, NRK_APP_STACKSIZE);
  RX_NODE_TASK.prio = 7;
  RX_NODE_TASK.FirstActivation = TRUE;
  RX_NODE_TASK.Type = BASIC_TASK;
  RX_NODE_TASK.SchType = NONPREEMPTIVE;
  RX_NODE_TASK.period.secs = 0;
  RX_NODE_TASK.period.nano_secs = 50*NANOS_PER_MS;
  RX_NODE_TASK.cpu_reserve.secs = 0;
  RX_NODE_TASK.cpu_reserve.nano_secs = 10*NANOS_PER_MS;
  RX_NODE_TASK.offset.secs = 0;
  RX_NODE_TASK.offset.nano_secs = 0;

  RX_SERV_TASK.task = rx_serv_task;
  nrk_task_set_stk(&RX_SERV_TASK, rx_serv_task_stack, NRK_APP_STACKSIZE);
  RX_SERV_TASK.prio = 6;
  RX_SERV_TASK.FirstActivation = TRUE;
  RX_SERV_TASK.Type = BASIC_TASK;
  RX_SERV_TASK.SchType = NONPREEMPTIVE;
  RX_SERV_TASK.period.secs = 0;
  RX_SERV_TASK.period.nano_secs = 100*NANOS_PER_MS;
  RX_SERV_TASK.cpu_reserve.secs = 0;
  RX_SERV_TASK.cpu_reserve.nano_secs = 10*NANOS_PER_MS;
  RX_SERV_TASK.offset.secs = 0;
  RX_SERV_TASK.offset.nano_secs = 0;

  TX_NET_TASK.task = tx_net_task;
  nrk_task_set_stk(&TX_NET_TASK, tx_net_task_stack, NRK_APP_STACKSIZE*8);
  TX_NET_TASK.prio = 4;
  TX_NET_TASK.FirstActivation = TRUE;
  TX_NET_TASK.Type = BASIC_TASK;
  TX_NET_TASK.SchType = NONPREEMPTIVE;
  TX_NET_TASK.period.secs = 0;
  TX_NET_TASK.period.nano_secs = 500*NANOS_PER_MS;
  TX_NET_TASK.cpu_reserve.secs = 0;
  TX_NET_TASK.cpu_reserve.nano_secs = 50*NANOS_PER_MS;
  TX_NET_TASK.offset.secs = 0;
  TX_NET_TASK.offset.nano_secs = 0;

  TX_SERV_TASK.task = tx_serv_task;
  nrk_task_set_stk(&TX_SERV_TASK, tx_serv_task_stack, NRK_APP_STACKSIZE);
  TX_SERV_TASK.prio = 4;
  TX_SERV_TASK.FirstActivation = TRUE;
  TX_SERV_TASK.Type = BASIC_TASK;
  TX_SERV_TASK.SchType = NONPREEMPTIVE;
  TX_SERV_TASK.period.secs = 1;
  TX_SERV_TASK.period.nano_secs = 0;
  TX_SERV_TASK.cpu_reserve.secs = 0;
  TX_SERV_TASK.cpu_reserve.nano_secs = 50*NANOS_PER_MS;
  TX_SERV_TASK.offset.secs = 0;
  TX_SERV_TASK.offset.nano_secs = 0;

  ALIVE_TASK.task = alive_task;
  nrk_task_set_stk(&ALIVE_TASK, alive_task_stack, NRK_APP_STACKSIZE);
  ALIVE_TASK.prio = 2;
  ALIVE_TASK.FirstActivation = TRUE;
  ALIVE_TASK.Type = BASIC_TASK;
  ALIVE_TASK.SchType = NONPREEMPTIVE;
  ALIVE_TASK.period.secs = 5;
  ALIVE_TASK.period.nano_secs = 0;
  ALIVE_TASK.cpu_reserve.secs = 0;
  ALIVE_TASK.cpu_reserve.nano_secs = 50*NANOS_PER_MS;
  ALIVE_TASK.offset.secs = 0;
  ALIVE_TASK.offset.nano_secs = 0;

  HAND_TASK.task = hand_task;
  nrk_task_set_stk(&HAND_TASK, hand_task_stack, NRK_APP_STACKSIZE);
  HAND_TASK.prio = 1;
  HAND_TASK.FirstActivation = TRUE;
  HAND_TASK.Type = BASIC_TASK;
  HAND_TASK.SchType = NONPREEMPTIVE;
  HAND_TASK.period.secs = 5;
  HAND_TASK.period.nano_secs = 0;
  HAND_TASK.cpu_reserve.secs = 0;
  HAND_TASK.cpu_reserve.nano_secs = 50*NANOS_PER_MS;
  HAND_TASK.offset.secs = 0;
  HAND_TASK.offset.nano_secs = 0;

  nrk_activate_task(&RX_NODE_TASK);
  nrk_activate_task(&RX_SERV_TASK);
  nrk_activate_task(&TX_SERV_TASK);
  nrk_activate_task(&TX_NET_TASK);
  nrk_activate_task(&ALIVE_TASK);
  nrk_activate_task(&HAND_TASK);

  nrk_kprintf(PSTR("Create done.\r\n"));
}

/***** END CONFIGURATION *****/
