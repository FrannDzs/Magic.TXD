/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/qtutils.h
*  PURPOSE:     Another utilities header for Qt stuff.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtGui/QContextMenuEvent>

void SetupWindowSize(QWidget *widget, unsigned int baseWidth, unsigned int baseHeight, unsigned int minWidth, unsigned int minHeight);

void RecalculateWindowSize(QWidget *widget, unsigned int baseWidth, unsigned int minWidth, unsigned int minHeight);

QHBoxLayout *CreateButtonsLayout(QWidget *Parent = 0);

QVBoxLayout *CreateRootLayout(QWidget *Parent = 0);

QHBoxLayout *CreateTopLevelHBoxLayout(QWidget *Parent = 0);

QVBoxLayout *CreateTopLevelVBoxLayout(QWidget *Parent = 0);

QPushButton *CreateButton(QString Text, QWidget *Parent = 0);

QWidget *CreateLine(QWidget *Parent = 0);

template <typename TopLayoutType> struct MagicLayout {
    QVBoxLayout *root;
    TopLayoutType *top;
    QHBoxLayout *bottom;

    MagicLayout(QWidget *Parent = 0) {
        root = CreateRootLayout(Parent);
        top = new TopLayoutType();
        top->setContentsMargins(QMargins(12, 12, 12, 12));
        top->setSpacing(10);
        root->addLayout(top);
        bottom = CreateButtonsLayout();
        QWidget *bottomWidget = new QWidget();
        bottomWidget->setObjectName("background_0");
        bottomWidget->setLayout(bottom);
        root->addWidget(CreateLine());
        root->addWidget(bottomWidget);
    }
};

class MagicLineEdit : public QLineEdit {
    void contextMenuEvent(QContextMenuEvent *e);
public:
    MagicLineEdit(QString str) : QLineEdit(str){}
    MagicLineEdit(){}
};
