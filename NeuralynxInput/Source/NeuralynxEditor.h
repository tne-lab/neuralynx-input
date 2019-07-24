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

#ifndef NEURALYNX_EDITOR_H_INCLUDED
#define NEURALYNX_EDITOR_H_INCLUDED

#include <EditorHeaders.h>
#include "NeuralynxThread.h"


class NeuralynxEditor : public GenericEditor, public Value::Listener
{
public:
    NeuralynxEditor(SourceNode* sn, NeuralynxThread* t);
    ~NeuralynxEditor();

    void startAcquisition() override;
    void stopAcquisition() override;

    IPAddress updateAndGetIPAddress();

    void valueChanged(Value& value) override;

private:
    NeuralynxThread* thread;

    Array<IPAddress> availableIPs;
    IPAddress selectedIP;

    // config
    ScopedPointer<Label> connectionLabel;
    ScopedPointer<ComboBox> addressBox;
    ScopedPointer<Label> portLabel;
    ScopedPointer<Label> portEditable;

    // status
    ScopedPointer<Label> receivingLabel;
    void updateReceivingLabel(bool isReceiving);

    ScopedPointer<Label> channelsLabel;
    void updateChannelsLabel(int numBoards);

    ScopedPointer<Label> hzLabel;
    void updateHzLabel(float sampleRate);

    ScopedPointer<UtilityButton> refreshButton;
    ScopedPointer<Label> refreshingLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NeuralynxEditor);
};

#endif // NEURALYNX_EDITOR_H_INCLUDED