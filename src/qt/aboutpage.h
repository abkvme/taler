// Copyright (c) 2024 The Taler developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_ABOUTPAGE_H
#define BITCOIN_QT_ABOUTPAGE_H

#include <QWidget>

class ClientModel;
class PlatformStyle;

class AboutPage : public QWidget
{
    Q_OBJECT

public:
    explicit AboutPage(const PlatformStyle *platformStyle, QWidget *parent = nullptr);

    void setClientModel(ClientModel *clientModel);

private:
    ClientModel *clientModel;
    const PlatformStyle *platformStyle;
};

#endif // BITCOIN_QT_ABOUTPAGE_H
