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

#ifndef NEURALYNX_THREAD_H_INCLUDED
#define NEURALYNX_THREAD_H_INCLUDED

#include <DataThreadHeaders.h>

class NeuralynxThread 
    : public DataThread
    , public Label::Listener
    , public Button::Listener
{
    friend class NeuralynxEditor;

public:
    NeuralynxThread(SourceNode* sn);
    ~NeuralynxThread();

    void resizeBuffers() override;

    bool updateBuffer() override;

    bool foundInputSource() override;

    bool startAcquisition() override;
    bool stopAcquisition() override;

    int getNumDataOutputs(DataChannel::DataChannelTypes type, int subProcessorIdx) const override;
    int getNumTTLOutputs(int subprocessorIdx) const override;

    float getSampleRate(int subProcessorIdx) const override;
    float getBitVolts(const DataChannel* chan) const override;

    bool usesCustomNames() const override;

    GenericEditor* createEditor(SourceNode* sn) override;

    String getChannelUnits(int chanIndex) const override;

    void labelTextChanged(Label* label) override;
    void buttonClicked(Button* button) override;

private:
    void setDefaultChannelNames() override;

    /*** local functions ***/

    void setNumBoards(int n);

    // Receive a packet, with unspecified # of boards. Blocks for a maximum of 5 ms (before it gives up).
    // On failure, returns 0; otherwise writes the packet to socketData and returns the # of boards.
    // Does not check the checksum.
    // If expectedBoards is > 0, returns 0 (fails) if the # of boards does not match this input.
    // Note that otherwise, it is possible that multiple packets will be received at once.
    // offsetWords is the offset in 32-bit words from the start of socketBuffer to write the packet to.
    int rcvPacket(int expectedBoards = 0, int offsetWords = 0);

    // Attempts to receive blockSize packets (using the current value of numBoards).
    // Returns true on success, false on failure.
    bool rcvBlock();

    // Attempts to (re)create the socket, destroying one if it already exists.
    void createAndBindSocket();

    void flushSocket();

    // Check the header fields and checksum of the given packet (with specified # of boards)
    static bool packetValid(const uint32* packet, int boards);

    static int wordsInPacketWithBoards(int numBoards);

    /*** constants ***/

    static const int atlasMaxInputUv = 131072;
    static const float atlasRawBitVolts; // for conversion from raw data (24-bit precision) to uV

    static const int minBoards = 1;
    static const int maxBoards = 16;

    static const int headerWords = 17;
    static const int boardChannels = 32;
    static const int footerWords = 1;

    static const int maxChannels = boardChannels * maxBoards;
    const int minPacketSize = wordsInPacketWithBoards(minBoards) * 4;
    const int maxPacketSize = wordsInPacketWithBoards(maxBoards) * 4;

    static const int blockSize = 20;

    static const uint16 defaultPort = 26090;

    static const int srcBufferSize = 10000;

    static const int timeoutMs = 50;

    /*** state ***/

    // Each board has 32 channels. Determined in foundInputSource.
    int numBoards;
    Value numBoardsValue;

    // Determined in foundInputSource.
    Value sampleRate;

    Value updateBoardsAndHz;

    // for use while thread is running
    bool firstSample;
    uint64 tsOffset;
    double usPerSamp;

    ScopedPointer<DatagramSocket> socket;
    IPAddress ipAddress;
    int port;

    Value receivingData;

    const int socketBufferSize = blockSize * maxPacketSize;
    const HeapBlock<uint32> socketBuffer{ socketBufferSize / sizeof(uint32) };

    const HeapBlock<float> thisBlock{ blockSize * maxChannels };

    uint64 invalidPackets; // keep track, maybe for debugging

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NeuralynxThread);
};

#endif // NEURALYNX_THREAD_H_INCLUDED