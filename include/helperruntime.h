/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/helperruntime.h
*  PURPOSE:     Helper runtime exports.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

// There are multiple types of help text possible.
enum class eHelperTextType
{
    DIALOG_WITH_TICK,           // dialog with the text centered, a checkbox to disable showing this item the second time and the OK button
    DIALOG_SHOW_ONCE            // dialog with the text centered and the OK button, showed only once, ever
};

bool RegisterHelperWidget(MainWindow *mainWnd, const char *triggerName, eHelperTextType diagType, const char *locale_item_name, bool richText = false );
bool UnregisterHelperWidget( MainWindow *mainWnd, const char *triggerName );

// The runtime is allowed to "ping" the system with a trigger, which should pop up help text.
void TriggerHelperWidget( MainWindow *mainWnd, const char *triggerName, QWidget *optParent = NULL );