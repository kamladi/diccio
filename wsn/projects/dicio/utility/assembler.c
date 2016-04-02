/**
 * 18-748 Wireless Sensor Networks
 * Spring 2016
 * Lab 3: Multi-Hop Communication
 * assembler.c
 * Kedar Amladi // kamladi. Daniel Santoro // ddsantor. Adam Selevan // aselevan.
 */

#include <assembler.h>


/*
Assemble packet to go to the server.
Right now the server is looking for ":"
*/
void assemble_serv_packet(uint8_t *tx_buf, packet *tx)
{
    switch(tx->type)
    {
        case MSG_CMD:
        {
            // msg_cmd from server has num_hops of 0.
            // cmd type message will only have "ON/OFF" payload value.
            // Will this ever happen?
            sprintf(tx_buf, "%d:%d:%d:%d:%d", tx->source_id, tx->seq_num, tx->type, tx->num_hops, tx->payload[0]);
            break;
        }

        case MSG_DATA:
        {
            sprintf(tx_buf, "%d:%d:%d:%d:%d,%d,%d", tx->source_id, tx->seq_num, tx->type, tx->num_hops,

                (uint16_t)tx->payload[DATA_PWR_INDEX], (uint16_t)tx->payload[DATA_TEMP_INDEX],
                 (uint16_t)tx->payload[DATA_LIGHT_INDEX], tx->payload[DATA_STATE_INDEX]);
            break;
        }

        case MSG_CMDACK:
        {
            sprintf(tx_buf, "%d:%d:%d:%d:%d,%d", tx->source_id, tx->seq_num, tx->type, tx->num_hops,
                (uint16_t)tx->payload[CMDACK_ID_INDEX], tx->payload[CMDACK_STATE_INDEX]);
            break;
        }

        case MSG_HANDACK: // assemble hand_ack to server
        {
            sprintf(tx_buf, "%d:%d:%d:%d:%d", tx->source_id, (uint16_t)tx->seq_num, tx->type, tx->num_hops,
                tx->payload[HANDACK_NODE_ID_INDEX]);
            break;
        }
    }
}

/*
Assemble packet to go to the network.
Use network format.
*/
uint8_t assemble_packet(uint8_t *tx_buf, packet *tx)
{
    uint8_t length = 0;
    switch(tx->type)
    {
        case MSG_CMD:
        {
            length = 9;
            // msg_cmd from server has hop_num of 0.
            // cmd type message will only have "ON/OFF" payload value.
            tx_buf[0] = tx->source_id;
            tx_buf[1] = tx->seq_num & 0xff;
            tx_buf[2] = (tx->seq_num >> 8) & 0xff;
            tx_buf[3] = tx->type;
            tx_buf[4] = tx->num_hops;
            tx_buf[5] = tx->payload[0];
            tx_buf[6] = tx->payload[1];
            tx_buf[7] = tx->payload[2];
            tx_buf[8] = tx->payload[3];
            break;
        }

        case MSG_CMDACK:
        {

            length = 8;
            tx_buf[0] = tx->source_id;
            tx_buf[1] = tx->seq_num & 0xff;
            tx_buf[2] = (tx->seq_num >> 8) & 0xff;
            tx_buf[3] = tx->type;
            tx_buf[4] = tx->num_hops;
            // insert cmd id
            tx_buf[5] = tx->payload[0];
            tx_buf[6] = tx->payload[1];
            // insert state
            tx_buf[7] = tx->payload[2];
            break;
        }

        case MSG_DATA:
        {
            length = 12;
            // NEED TO TEST/DEVELOP!!
            // msg_cmd from server has hop_num of 0.
            // cmd type message will only have "ON/OFF" payload value.
            tx_buf[0] = tx->source_id;
            tx_buf[1] = tx->seq_num & 0xff;
            tx_buf[2] = (tx->seq_num >> 8) & 0xff;
            tx_buf[3] = tx->type;
            tx_buf[4] = tx->num_hops;
            tx_buf[5] = tx->payload[0];
            tx_buf[6] = tx->payload[1];
            tx_buf[7] = tx->payload[2];
            tx_buf[8] = tx->payload[3];
            tx_buf[9] = tx->payload[4];
            tx_buf[10] = tx->payload[5];
            tx_buf[11] = tx->payload[6];
            break;
        }

        case MSG_HAND:
        {
            printf("hand asm \r\n");
            length = 5;
            tx_buf[0] = tx->source_id;
            tx_buf[1] = tx->seq_num & 0xff;
            tx_buf[2] = (tx->seq_num >> 8) & 0xff;
            tx_buf[3] = tx->type;
            tx_buf[4] = tx->num_hops;
            break;
        }

        // assemble handack msg for the new node
        case MSG_HANDACK:
        {
            length = 6;
            tx_buf[0] = tx->source_id;
            tx_buf[1] = tx->seq_num & 0xff;
            tx_buf[2] = (tx->seq_num >> 8) & 0xff;
            tx_buf[3] = tx->type; //make sure this is HANDACK (9)
            tx_buf[4] = tx->num_hops;
            tx_buf[5] = tx->payload[0];
            //printf("asm ack: %d:%d:%d:%d:%d\r\n", tx_buf[0], tx_buf[1], tx_buf[2], tx_buf[3], tx_buf[4]);
            break;
        }
    }
    return length;
}
