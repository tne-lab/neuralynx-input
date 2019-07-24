/*
------------------------------------------------------------------

This file is part of a plugin for the Open Ephys GUI
Copyright (C) 2018 Translational NeuroEngineering Laboratory

------------------------------------------------------------------

We hope that this plugin will be useful to others, but its source code
and functionality are subject to a non-disclosure agreement (NDA) with
Neuralynx, Inc. If you or your institution have not signed the appropriate
NDA, STOP and do not read or execute this plugin until you have done so.
Do not share this plugin with other parties who have not signed the NDA.

*/

#include "NeuralynxThread.h"
#include "NeuralynxEditor.h"

#include <sstream> // for reading port label

NeuralynxThread::NeuralynxThread(SourceNode* s)
    : DataThread        (s)
    , numBoards         (1)
    , numBoardsValue    (1)
    , sampleRate        (s->getDefaultSampleRate())
    , updateBoardsAndHz (var(false))
    , port              (defaultPort)
    , receivingData     (var(false))
    , invalidPackets    (0)
{
    sourceBuffers.add(new DataBuffer(numBoards * boardChannels, srcBufferSize));
}


NeuralynxThread::~NeuralynxThread()
{}


void NeuralynxThread::resizeBuffers()
{
    sourceBuffers[0]->resize(numBoards * boardChannels, srcBufferSize);

    timestamps.resize(blockSize);
    ttlEventWords.resize(blockSize);
}


const float NeuralynxThread::atlasRawBitVolts = NeuralynxThread::atlasMaxInputUv / float(1 << 23);

bool NeuralynxThread::updateBuffer()
{
    if (firstSample)
    {
        // flush socket one last time before starting acquisition
        flushSocket();
    }

    if (!rcvBlock())
    {
        return false;
    }

    int numChans = numBoards * boardChannels;
    int numSamples = blockSize;
    int sOut = 0;
    for (int sIn = 0; sIn < blockSize; ++sIn)
    {
        const uint32* packetStart = socketBuffer + wordsInPacketWithBoards(numBoards) * sIn;

        if (!packetValid(packetStart, numBoards))
        {
            // skip, don't stop acquiring though since it might just be a randomly flipped bit
            ++invalidPackets;
            --numSamples;
            continue;
        }

        // get timestamp
        uint64 tsRaw = (uint64(ByteOrder::littleEndianInt(packetStart + 3)) << 32)
            + ByteOrder::littleEndianInt(packetStart + 4);

        if (firstSample)
        {
            firstSample = false;
            tsOffset = tsRaw;
            timestamps.setUnchecked(sOut, 0);
        }
        else
        {
            int64 tsDiff = tsRaw - tsOffset;
            int64 ts = int64((tsDiff + usPerSamp / 2) / usPerSamp); // (round to nearest)
            timestamps.setUnchecked(sOut, ts);
        }

        // get ttl
        ttlEventWords.setUnchecked(sOut, ByteOrder::littleEndianInt(packetStart + 6));

        // get data
        for (int c = 0; c < numChans; ++c)
        {
            uint32 uSamp = ByteOrder::littleEndianInt(packetStart + headerWords + c);
            thisBlock[numChans * sOut + c] = *reinterpret_cast<int32*>(&uSamp) * atlasRawBitVolts;
        }

        ++sOut;
    }

    sourceBuffers[0]->addToBuffer(thisBlock, &timestamps.getReference(0), &ttlEventWords.getReference(0), numSamples);
    return true;
}


bool NeuralynxThread::foundInputSource()
{
    auto ed = static_cast<NeuralynxEditor*>(sn->getEditor());
    if (ed == nullptr)
    {
        receivingData = false;
        return false;
    }
    
    IPAddress newIP = ed->updateAndGetIPAddress();

    if (newIP == IPAddress())
    {
        receivingData = false;
        return false;
    }

    if (socket == nullptr || ipAddress != newIP || socket->getBoundPort() != port)
    {
        // make and bind a new socket
        ipAddress = newIP;
        createAndBindSocket();

        if (socket == nullptr)
        {
            receivingData = false;
            return false;
        }
    }

    jassert(socket->getBoundPort() == port);

    flushSocket();

    bool update = updateBoardsAndHz.getValue();
    updateBoardsAndHz = false;

    // receive a test packet and check the number of boards
    int boards = rcvPacket();
    bool valid = boards > 0 && packetValid(socketBuffer, boards);

    if (valid && (!receivingData.getValue() || update))
    {
        if (boards != numBoards)
        {
            setNumBoards(boards);
        }

        // try to infer the sample rate by receiving samples over 100 ms
        int numRcvd = 0;
        uint32 stopTs = 0;
        while (true)
        {
            if (!rcvPacket(boards))
            {
                valid = false;
                break;
            }

            uint32 ts = ByteOrder::littleEndianInt(socketBuffer + 4); // (just lower-order 32 bits)
            
            if (stopTs == 0)
            {
                stopTs = ts + 100000;
            }
            else if (ts >= stopTs)
            {
                break;
            }

            numRcvd++;
        }

        if (valid)
        {
            // deal with 32,768 Hz as a special case (see documentation for "-CreateHardwareSubSystem")
            if (numRcvd >= 3239 && numRcvd <= 3338)
            {
                sampleRate = 32768;
            }
            else // must be multiple of 2000 Hz
            {
                float srate = float(((numRcvd + 100) / 200) * 2000);
                jassert(srate >= 16000 && srate <= 40000); // ATLAS limits
                sampleRate = srate;
            }
        }
    }

    receivingData = valid;
    return valid;
}


bool NeuralynxThread::startAcquisition()
{
    updateBoardsAndHz = false;
    firstSample = true;
    usPerSamp = 1000000 / double(sampleRate.getValue());
    startThread();
    return true;
}


bool NeuralynxThread::stopAcquisition()
{
    bool ok = true;

    if (getCurrentThread() != this) // if an acquisition error occurs, will be called from the thread
    {
        ok = stopThread(500);
    }
    else
    {
        // if an error ocurred, we should refresh the socket.
        createAndBindSocket();
    }

    sourceBuffers[0]->clear();
    return ok;
}


int NeuralynxThread::getNumDataOutputs(DataChannel::DataChannelTypes type, int subProcessorIdx) const
{
    if (subProcessorIdx == 0 && type == DataChannel::HEADSTAGE_CHANNEL)
    {
        return numBoards * boardChannels;
    }
    return 0;
}


int NeuralynxThread::getNumTTLOutputs(int subprocessorIdx) const
{
    if (subprocessorIdx == 0)
    {
        return 32;
    }
    return 0;
}


float NeuralynxThread::getSampleRate(int subprocessorIdx) const
{
    if (subprocessorIdx == 0)
    {
        return sampleRate.getValue();
    }
    return 0;
}


float NeuralynxThread::getBitVolts(const DataChannel* chan) const
{
    // Even though technically the input to ATLAS can be in the range +/-131,072 uV,
    // and the data sent over UDP are scaled to reflect that, the processed data are stored
    // in the CSC files as int16s in uV, i.e. with a bit(u)volts value of 1 and range of
    // +/-32,767 uV. Thus it should be safe to use the same bitvolts of 1 when saving data from OEP.
    return 1;
}


bool NeuralynxThread::usesCustomNames() const
{
    return true;
}


GenericEditor* NeuralynxThread::createEditor(SourceNode* sn)
{
    return new NeuralynxEditor(sn, this);
}


String NeuralynxThread::getChannelUnits(int chanIndex) const
{
    return "uV";
}



void NeuralynxThread::labelTextChanged(Label* label)
{
    // the only label that should trigger this is the portLabel
    std::istringstream portInput(label->getText().toStdString());
    uint16 newPort;
    portInput >> newPort;
    if (portInput.good() && newPort != 0)
    {
        port = newPort;
    }
    else
    {
        CoreServices::sendStatusMessage("Neuralynx Input: invalid port");
        label->setText(String(port), dontSendNotification);
    }
}


void NeuralynxThread::buttonClicked(Button* button)
{
    // must be the refresh button
    if (!CoreServices::getAcquisitionStatus())
    {
        updateBoardsAndHz = true;
    }
}


void NeuralynxThread::setDefaultChannelNames()
{
    for (int c = 0; c < numBoards * boardChannels; ++c)
    {
        ChannelCustomInfo info;
        info.name = "CH" + String(c + 1);
        info.gain = getBitVolts(sn->getDataChannel(c));
        channelInfo.set(c, info);
    }
}


void NeuralynxThread::setNumBoards(int n)
{
    if (CoreServices::getAcquisitionStatus() || n < minBoards || n > maxBoards)
    {
        jassertfalse;
        return;
    }

    if (n == numBoards)
    {
        return;
    }

    numBoards = n;
    numBoardsValue = n;
    sn->requestChainUpdate();
}


int NeuralynxThread::rcvPacket(int expectedBoards, int offsetWords)
{
    if (expectedBoards > maxBoards) { return 0; }

    int bytesToRead = expectedBoards > 0
        ? wordsInPacketWithBoards(expectedBoards) * 4
        : maxPacketSize;

    if (offsetWords * 4 + bytesToRead > socketBufferSize)
    {
        jassertfalse; // overrun!!
        return 0;
    }

    int bytesRcvd;

    // try to receive a packet until timeout is reached
    uint32 t1 = Time::getMillisecondCounter();
    while (Time::getMillisecondCounter() - t1 < timeoutMs &&
           0 == (bytesRcvd = socket->read(socketBuffer + offsetWords, bytesToRead, false)));

    if (expectedBoards > 0)
    {
        return bytesRcvd == bytesToRead ? expectedBoards : 0;
    }

    // figure out # of boards
    if (bytesRcvd < minPacketSize) { return 0; }

    int32 reportedChans = ByteOrder::littleEndianInt(socketBuffer + offsetWords + 2) - 10;

    if (reportedChans % boardChannels != 0)
    {
        return 0;
    }

    int boards = reportedChans / boardChannels;
    if (bytesRcvd < wordsInPacketWithBoards(boards) * 4
        || boards < minBoards || boards > maxBoards)
    {
        return 0;
    }

    return boards;
}


bool NeuralynxThread::rcvBlock()
{
    int boards = numBoards;
    int packetLength = wordsInPacketWithBoards(boards);

    for (int s = 0; s < blockSize; ++s)
    {
        if (rcvPacket(boards, s * packetLength) != boards)
        {
            return false;
        }
    }
    return true;
}


void NeuralynxThread::createAndBindSocket()
{
    socket = new DatagramSocket();

    if (!socket->bindToPort(port, ipAddress.toString()))
    {
        socket = nullptr;
    }
}


void NeuralynxThread::flushSocket()
{
    int size = 65536; // UDP buffer size
    MemoryBlock devnull(size);
    while (socket->read(devnull.getData(), size, false) != 0);
}


bool NeuralynxThread::packetValid(const uint32* packet, int boards)
{
    // expected header values (see here: https://neuralynx.com/software/NeuralynxDataFileFormats.pdf)
    if (ByteOrder::littleEndianInt(packet) != 2048
        || ByteOrder::littleEndianInt(packet + 1) != 1
        || ByteOrder::littleEndianInt(packet + 2) != boards * boardChannels + 10)
    {
        return false;
    }
    
    // checksum
    int packetLength = wordsInPacketWithBoards(boards);

    uint32 crcValue = 0;
    for (int i = 0; i < packetLength; ++i)
    {
        crcValue ^= packet[i];
    }
    
    return crcValue == 0;
}


int NeuralynxThread::wordsInPacketWithBoards(int numBoards)
{
    return headerWords + numBoards * boardChannels + footerWords;
};