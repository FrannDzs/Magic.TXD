/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/qtutils.cpp
*  PURPOSE:     Utilities for Qt interfacing.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"
#include "qtutils.h"

void RecalculateWindowSize(QWidget *widget, unsigned int baseWidth, unsigned int minWidth, unsigned int minHeight) {
    unsigned int minwidth = minWidth;

    if (baseWidth > minWidth)
        minwidth = baseWidth;

    widget->setMinimumSize(minwidth, minHeight);

    unsigned int new_w = widget->width();
    if (new_w < minwidth)
        new_w = minwidth;

    widget->resize(new_w, widget->height());
}

void SetupWindowSize(QWidget *widget, unsigned int baseWidth, unsigned int baseHeight, unsigned int minWidth, unsigned int minHeight) {
    widget->setMinimumSize(minWidth, minHeight);
    widget->resize(baseWidth, baseHeight);
}

QHBoxLayout *CreateButtonsLayout(QWidget *Parent) {
    QHBoxLayout *buttonLayout = new QHBoxLayout(Parent);
    buttonLayout->setContentsMargins(QMargins(12, 12, 12, 12));
    buttonLayout->setAlignment(Qt::AlignBottom | Qt::AlignRight);
    buttonLayout->setSpacing(10);
    return buttonLayout;
}

QVBoxLayout *CreateRootLayout(QWidget *Parent) {
    QVBoxLayout *rootLayout = new QVBoxLayout(Parent);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->setSizeConstraint(QLayout::SetFixedSize);
    return rootLayout;
}

QHBoxLayout *CreateTopLevelHBoxLayout(QWidget *Parent) {
    QHBoxLayout *topLevelLayout = new QHBoxLayout(Parent);
    topLevelLayout->setContentsMargins(QMargins(12, 12, 12, 12));
    topLevelLayout->setSpacing(10);
    return topLevelLayout;
}

QVBoxLayout *CreateTopLevelVBoxLayout(QWidget *Parent) {
    QVBoxLayout *topLevelLayout = new QVBoxLayout(Parent);
    topLevelLayout->setContentsMargins(QMargins(12, 12, 12, 12));
    topLevelLayout->setSpacing(10);
    return topLevelLayout;
}

// See CreateButtonL for localized version.
QPushButton *CreateButton(QString Text, QWidget *Parent) {
    QPushButton *button = new QPushButton(Text, Parent);
    button->setMinimumWidth(90);
    return button;
}

QWidget *CreateLine(QWidget *Parent) {
    QWidget *line = new QWidget(Parent);
    line->setFixedHeight(1);
    line->setObjectName("hLineBackground");
    return line;
}

void MagicLineEdit::contextMenuEvent(QContextMenuEvent *e)
{
    QMenu *menu = QLineEdit::createStandardContextMenu();
    if (menu == nullptr)
        return QLineEdit::contextMenuEvent(e);

    QList<QAction*> actions = menu->actions();

    foreach(QAction *action, actions)
    {
        QString actionText = action->text();
        if (actionText.contains("Undo"))
        {
            actionText.replace("Undo", MAGIC_TEXT("Main.Undo"));
            action->setText(actionText);
        }
        else if (actionText.contains("Redo"))
        {
            actionText.replace("Redo", MAGIC_TEXT("Main.Redo"));
            action->setText(actionText);
        }
        else if (actionText.contains("Cu&t"))
        {
            actionText.replace("Cu&t", MAGIC_TEXT("Main.Cut"));
            action->setText(actionText);
        }
        else if (actionText.contains("Copy"))
        {
            actionText.replace("Copy", MAGIC_TEXT("Main.Copy"));
            action->setText(actionText);
        }
        else if (actionText.contains("Paste"))
        {
            actionText.replace("Paste", MAGIC_TEXT("Main.Paste"));
            action->setText(actionText);
        }
        else if (actionText.contains("Delete"))
        {
            actionText.replace("Delete", MAGIC_TEXT("Main.Delete"));
            action->setText(actionText);
        }
        else if (actionText.contains("Select All"))
        {
            actionText.replace("Select All", MAGIC_TEXT("Main.SelectA"));
            action->setText(actionText);
        }
    }

    menu->exec(e->globalPos());
}
