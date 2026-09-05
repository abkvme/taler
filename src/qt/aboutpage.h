// Copyright (c) 2024 The Taler developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_ABOUTPAGE_H
#define BITCOIN_QT_ABOUTPAGE_H

#include <QVector>
#include <QWidget>

class QHBoxLayout;
class QVBoxLayout;

class ClientModel;
class PlatformStyle;

class AboutPage : public QWidget
{
    Q_OBJECT

public:
    explicit AboutPage(const PlatformStyle *platformStyle, QWidget *parent = nullptr);

    void setClientModel(ClientModel *clientModel);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    /** Put the sections in one column or two, depending on the width available.
     *
     * Project is much the longest section, so it takes a column of its own and
     * the three short ones stack beside it. Below the threshold everything goes
     * back to a single column rather than squeezing two narrow ones.
     */
    void arrangeSections();

    ClientModel *clientModel;
    const PlatformStyle *platformStyle;

    QVector<QWidget*> m_sections;
    QHBoxLayout *m_columns = nullptr;
    QVBoxLayout *m_left_column = nullptr;
    QVBoxLayout *m_right_column = nullptr;
    //! -1 until the first arrangement, so the first resize always applies one.
    int m_column_count = -1;
};

#endif // BITCOIN_QT_ABOUTPAGE_H
