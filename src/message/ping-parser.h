#pragma once

#include <inttypes.h>
#include "ping-message.h"

/**
 * @brief Parser that digests data and notifies owner when something interesting happens
 *
 */
class PingParser
{
public:
    PingParser(uint16_t bufferLength = 512) : rxMessage(bufferLength), rxBuffer_(rxMessage.msgData) {}
    ~PingParser() = default;

    ping_message rxMessage; // This message is used as the rx buffer

     /**
     * @brief Number of messages/packets successfully parsed
     *
     */
    uint32_t parsed = 0; // number of messages/packets successfully parsed

     /**
     * @brief Number of parse errors
     *
     */
    uint32_t errors = 0; // number of parse errors

    // This enum MUST be contiguous
    enum class ParseState : uint8_t {
        NEW_MESSAGE,   // Just got a complete checksum-verified message
        WAIT_START,    // Waiting for the first character of a message 'B'
        WAIT_HEADER,   // Waiting for the second character in the two-character sequence 'BR'
        WAIT_LENGTH_L, // Waiting for the low byte of the payload length field
        WAIT_LENGTH_H, // Waiting for the high byte of the payload length field
        WAIT_MSG_ID_L, // Waiting for the low byte of the payload id field
        WAIT_MSG_ID_H, // Waiting for the high byte of the payload id field
        WAIT_SRC_ID,   // Waiting for the source device id
        WAIT_DST_ID,   // Waiting for the destination device id
        WAIT_PAYLOAD,  // Waiting for the last byte of the payload to come in
        WAIT_CHECKSUM_L, // Waiting for the checksum low byte
        WAIT_CHECKSUM_H, // Waiting for the checksum high byte
        ERROR, // Checksum didn't check out
    };

    // variables used for parsing
private:
    uint8_t* rxBuffer_;
    uint32_t rxBufferLength_;
    uint32_t rxCount_ = 0;
    uint16_t payloadLength_ = 0;
    ParseState state_ = ParseState::WAIT_START;

public:
    /**
     * @brief Reset parser state
     *
     */
    void reset()
    {
        state_ = ParseState::WAIT_START;
    }

    /**
     * @brief Parse a single byte
     *
     * @param b
     * @return ParseState
     */
    ParseState parseByte(const uint8_t b)
    {
        switch(state_) {
        case ParseState::WAIT_START:
            rxCount_ = 0;
            if (b == 'B') {
                rxBuffer_[rxCount_++] = b;
                state_++;
            }
            break;
        case ParseState::WAIT_HEADER:
            if (b == 'R') {
                rxBuffer_[rxCount_++] = b;
                state_++;
            } else {
                state_ = WAIT_START;
            }
            break;
        case ParseState::WAIT_LENGTH_L:
            rxBuffer_[rxCount_++] = b;
            payloadLength_ = b;
            state_++;
            break;
        case ParseState::WAIT_LENGTH_H:
            rxBuffer_[rxCount_++] = b;
            payloadLength_ = static_cast<uint16_t>((b << 8) | payloadLength_);
            if (payloadLength_ <= rxBufferLength_ - 8 - 2) {
                state_++;
            } else {
                state_ = WAIT_START;
            }
            break;
        case ParseState::WAIT_MSG_ID_L: // fall-through
        case ParseState::WAIT_MSG_ID_H:
        case ParseState::WAIT_SRC_ID:
        case ParseState::WAIT_DST_ID:
            rxBuffer_[rxCount_++] = b;
            state_++;
            if (payloadLength_ == 0) {
                // no payload bytes, so we skip WAIT_PAYLOAD state
                state_++;
            }
            break;
        case ParseState::WAIT_PAYLOAD:
            rxBuffer_[rxCount_++] = b;
            if (--payloadLength_ == 0) {
                state_++;
            }
            break;
        case ParseState::WAIT_CHECKSUM_L:
            rxBuffer_[rxCount_++] = b;
            state_++;
            break;
        case ParseState::WAIT_CHECKSUM_H:
            rxBuffer_[rxCount_++] = b;
            state_ = ParseState::WAIT_START;
            if (rxMessage.verifyChecksum()) {
                parsed++;
                return ParseState::NEW_MESSAGE;
            } else {
                errors++;
                return ParseState::ERROR;
            }
        }
        return state_;
    }
};
