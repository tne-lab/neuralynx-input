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

#include "NeuralynxEditor.h"


NeuralynxEditor::NeuralynxEditor(SourceNode* sn, NeuralynxThread* t)
    : GenericEditor(sn, false)
    , thread(t)
{
    desiredWidth = 210;
    
    // connection controls

    connectionLabel = new Label("ConnectionL", "Data connection:");
    connectionLabel->setBounds(8, 25, desiredWidth, 20);
    addAndMakeVisible(connectionLabel);
    
    addressBox = new ComboBox("IPAddressBox");
    addressBox->setBounds(10, 45, 125, 20);
    addressBox->setTooltip("IP address of this computer's network connection to the "
        "Digital Lynx SX or ATLAS system. This is 192.168.3.100 by default, "
        "and you probably need to configure a connection to the acquisition system with this "
        "IP address in order to receive data. (You will also have to clone the MAC address from the " 
        "computer running Cheetah or Pegasus.) If you have an advanced setup that uses "
        "a different IP address, you can select that here.");
    // no listener for this ComboBox, it gets handled when updateAndGetIPAddress is called.
    addAndMakeVisible(addressBox);

    portLabel = new Label("PortL", ":");
    portLabel->setBounds(133, 45, 15, 20);
    portLabel->setFont(portLabel->getFont().boldened());
    addAndMakeVisible(portLabel);

    portEditable = new Label("PortE", String(t->port));
    portEditable->setBounds(145, 45, 45, 20);
    portEditable->setTooltip("Port of this computer's connection to the Digital Lynx SX or ATLAS system. "
        "This should be 26090 unless you have an advanced setup that is not using the default.");
    portEditable->setEditable(true);
    portEditable->setColour(Label::ColourIds::backgroundColourId, Colours::lightgrey);
    portEditable->addListener(t);
    addAndMakeVisible(portEditable);

    // status indicators

    channelsLabel = new Label("ChannelsL");
    channelsLabel->setBounds(8, 85, 85, 18);
    t->numBoardsValue.addListener(this);
    updateChannelsLabel(t->numBoards);
    addChildComponent(channelsLabel);

    hzLabel = new Label("HzL");
    hzLabel->setBounds(90, 85, 100, 18);
    t->sampleRate.addListener(this);
    updateHzLabel(t->sampleRate.getValue());
    addChildComponent(hzLabel);

    receivingLabel = new Label("ReceivingL");
    receivingLabel->setBounds(8, 65, desiredWidth, 18);
    t->receivingData.addListener(this);

    refreshButton = new UtilityButton("REFRESH", Font());
    refreshButton->setBounds(10, 105, 75, 20);
    refreshButton->addListener(t);
    addChildComponent(refreshButton);

    refreshingLabel = new Label("RefreshingL", "...");
    refreshingLabel->setFont(refreshingLabel->getFont().boldened().withHeight(20));
    refreshingLabel->setBounds(90, 100, 50, 20);
    t->updateBoardsAndHz.addListener(this);
    addChildComponent(refreshingLabel);

    // show or hide components based on whether we are receiving data
    bool receiving = t->receivingData.getValue();

    channelsLabel->setVisible(receiving);
    hzLabel      ->setVisible(receiving);
    refreshButton->setVisible(receiving);

    refreshingLabel->setVisible(receiving && t->updateBoardsAndHz.getValue());
    
    updateReceivingLabel(receiving);
    addAndMakeVisible(receivingLabel);
}


NeuralynxEditor::~NeuralynxEditor() {}


void NeuralynxEditor::startAcquisition()
{
    addressBox->setEnabled(false);
    portEditable->setEnabled(false);
    refreshButton->setEnabled(false);
}


void NeuralynxEditor::stopAcquisition()
{
    addressBox->setEnabled(true);
    portEditable->setEnabled(true);
    refreshButton->setEnabled(true);
}


IPAddress NeuralynxEditor::updateAndGetIPAddress()
{
    static const IPAddress defaultIP({ 192, 168, 3, 100 });

    jassert(addressBox->getNumItems() == availableIPs.size()); // this is an invariant
    jassert(addressBox->getNumItems() == 0 || addressBox->getSelectedId() > 0);

    if (addressBox->getNumItems() > 0)
    {
        selectedIP = availableIPs[addressBox->getSelectedId() - 1];
    }

    Array<IPAddress> currList;
    IPAddress::findAllAddresses(currList);
    if (currList == availableIPs)
    {
        return selectedIP;
    }

    availableIPs.swapWith(currList);

    // update the combobox
    {
        const MessageManagerLock mmLock;

        addressBox->clear();
        int length = availableIPs.size();

        if (length == 0)
        {
            selectedIP = IPAddress();
            return selectedIP;
        }

        int selectedIPInd = -1; // look for currently selected address, if it's nonempty
        int defaultIPInd = -1;  // look for default IP address
        for (int i = 0; i < length; ++i)
        {
            IPAddress thisIP = availableIPs[i];
            if (selectedIP != IPAddress() && thisIP == selectedIP)
            {
                selectedIPInd = i;
            }

            if (thisIP == defaultIP)
            {
                defaultIPInd = i;
            }

            addressBox->addItem(thisIP.toString(), i + 1);

        }

        int newIPInd = 0; // default to first one in the list
        if (selectedIPInd != -1)
        {
            newIPInd = selectedIPInd;
        }
        else if (defaultIPInd != -1)
        {
            newIPInd = defaultIPInd;
        }

        addressBox->setSelectedId(newIPInd + 1);
        selectedIP = availableIPs[newIPInd];
        return selectedIP;
    }
}


void NeuralynxEditor::valueChanged(Value& value)
{
    if (value.refersToSameSourceAs(thread->receivingData))
    {
        bool isReceiving = value.getValue();
        updateReceivingLabel(isReceiving);
        channelsLabel->setVisible(isReceiving);
        hzLabel->setVisible(isReceiving);
        refreshButton->setVisible(isReceiving);
        refreshingLabel->setVisible(isReceiving && thread->updateBoardsAndHz.getValue());
    }
    else if (value.refersToSameSourceAs(thread->numBoardsValue))
    {
        updateChannelsLabel(int(value.getValue()));
    }
    else if (value.refersToSameSourceAs(thread->sampleRate))
    {
        updateHzLabel(float(value.getValue()));
    }
    else if (value.refersToSameSourceAs(thread->updateBoardsAndHz))
    {
        refreshingLabel->setVisible(value.getValue() && thread->receivingData.getValue());

        float srate = thread->sampleRate.getValue();
    
        //// Need to tell the rest of the signal chain if the refresh button is clicked. 
        //// Make sure sample rate is a valid value so we don't send 0 which sometimes gets inferred at first
        if (srate >= 16000 && srate <= 40000)
        {
            std::cout << "Neuralynx plugin sending signal chain update for sample rate:" << (float)thread->sampleRate.getValue() << std::endl;
            CoreServices::updateSignalChain(this);
        }
    }
}


void NeuralynxEditor::updateReceivingLabel(bool isReceiving)
{
    if (isReceiving)
    {
        receivingLabel->setText("Receiving:", dontSendNotification);
    }
    else
    {
        receivingLabel->setText("Not receiving", dontSendNotification);
    }
}


void NeuralynxEditor::updateChannelsLabel(int numBoards)
{
    channelsLabel->setText(String(numBoards * thread->boardChannels) + " channels", dontSendNotification);
}


void NeuralynxEditor::updateHzLabel(float sampleRate)
{
    hzLabel->setText("@ " + String(sampleRate) + " Hz", dontSendNotification);
}