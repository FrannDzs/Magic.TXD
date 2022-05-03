/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/massconvert.h
*  PURPOSE:     Header of the TxdGen UI.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

#include <QtWidgets/QDialog>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QComboBox>

#include "languages.h"

#include "progresslogedit.h"
#include "qtutils.h"

struct MassConvertWindow;

struct MassConvertWindow : public QDialog, public magicTextLocalizationItem
{
    friend struct massconvEnv;

public:
    MassConvertWindow( MainWindow *mainwnd );
    ~MassConvertWindow();

    void postLogMessage( QString msg );

    MainWindow *mainwnd;

public slots:
    void OnRequestConvert( bool checked );
    void OnRequestCancel( bool checked );

protected:
    void updateContent( MainWindow *mainWnd ) override;

    void customEvent( QEvent *evt ) override;

private:
    void serialize( void );

    // Widget pointers.
    MagicLineEdit *editGameRoot;
    MagicLineEdit *editOutputRoot;
    QComboBox *selPlatformBox;
    QComboBox *selGameBox;
    QCheckBox *propClearMipmaps;
    QCheckBox *propGenMipmaps;
    MagicLineEdit *propGenMipmapsMax;
    QCheckBox *propImproveFiltering;
    QCheckBox *propCompressTextures;
    QCheckBox *propReconstructIMG;
    QCheckBox *propCompressedIMG;

    ProgressLogEdit logEditControl;

    QPushButton *buttonConvert;

public:
    volatile rw::thread_t conversionThread;

    rw::rwlock *volatile convConsistencyLock;

    RwListEntry <MassConvertWindow> node;
};
