/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdlog.cpp
*  PURPOSE:     Log of the editor.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"
#include <QtCore/QTextStream>
#include "qtutils.h"
#include "languages.h"

TxdLog::TxdLog(MainWindow *mainWnd, QString AppPath, QWidget *ParentWidget)
{
	this->mainWnd = mainWnd;

	parent = ParentWidget;
	positioned = false;
	logWidget = new QWidget(ParentWidget, Qt::Window);
	//logWidget->setMinimumSize(450, 150);
	//logWidget->resize(500, 200);
	logWidget->setObjectName("background_1");
	/* --- Top panel --- */

	QPushButton *buttonSave = CreateButton("");

	this->buttonSave = buttonSave;

	connect(buttonSave, &QPushButton::clicked, this, &TxdLog::onLogSaveRequest);

	buttonSave->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

	QPushButton *buttonCopy = CreateButton("");

	this->buttonCopy = buttonCopy;

	connect(buttonCopy, &QPushButton::clicked, this, &TxdLog::onCopyLogLinesRequest);

	buttonSave->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

	QPushButton *buttonCopyAll = CreateButton("");

	this->buttonCopyAll = buttonCopyAll;

	connect(buttonCopyAll, &QPushButton::clicked, this, &TxdLog::onCopyAllLogLinesRequest);

	buttonCopyAll->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

	QPushButton *buttonClear = CreateButton(":3");

	this->buttonClear = buttonClear;

	connect(buttonClear, &QPushButton::clicked, this, &TxdLog::onLogClearRequest);

	buttonClear->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

	QPushButton *buttonClose = CreateButton("");

	this->buttonClose = buttonClose;

	connect(buttonClose, &QPushButton::clicked, this, &TxdLog::onWindowHideRequest);

	buttonClose->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

	QHBoxLayout *buttonsLayout = new QHBoxLayout;
	buttonsLayout->addWidget(buttonSave);
	buttonsLayout->addWidget(buttonCopy);
	buttonsLayout->addWidget(buttonCopyAll);
	buttonsLayout->addWidget(buttonClear);
	buttonsLayout->addWidget(buttonClose);

	QWidget *buttonsBackground = new QWidget();
	buttonsBackground->setFixedHeight(60);
	buttonsBackground->setObjectName("background_1");
	buttonsBackground->setLayout(buttonsLayout);
	buttonsBackground->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	QWidget *hLineBackground = new QWidget();
	hLineBackground->setFixedHeight(1);
	hLineBackground->setObjectName("hLineBackground");
	QVBoxLayout *mainLayout = new QVBoxLayout();
	mainLayout->addWidget(buttonsBackground);
	mainLayout->addWidget(hLineBackground);

	/* --- List --- */
	listWidget = new QListWidget;
	listWidget->setObjectName("logList");
	mainLayout->addWidget(listWidget);
	listWidget->setSelectionMode(QAbstractItemView::SelectionMode::MultiSelection);

	// Still not fure, but looks like weird bug with scrollbars was fixed in Qt5.x
	//listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->setMargin(0);
	mainLayout->setSpacing(0);
	logWidget->setLayout(mainLayout);

	// icons
	// NOTE: the Qt libpng implementation complains about a "known invalid sRGB profile" here.
	picWarning.load(AppPath + "/resources/warning.png");
	picError.load(AppPath + "/resources/error.png");
	picInfo.load(AppPath + "/resources/info.png");

	SetupWindowSize(logWidget, 450, 200, 450, 150);

	RegisterTextLocalizationItem( this );
}

TxdLog::~TxdLog( void )
{
	UnregisterTextLocalizationItem( this );
}

void TxdLog::updateContent( MainWindow *mainWnd )
{
	logWidget->setWindowTitle( MAGIC_TEXT( "Main.Log.Desc" ) );

	unsigned int menuWidth =  5 * 20 * 2 + 10 * 6;  // oh god no

	QString sLogSave = MAGIC_TEXT("Main.Log.Save");
	menuWidth += GetTextWidthInPixels(sLogSave, 20);

	buttonSave->setText( sLogSave );

	QString sLogCopy = MAGIC_TEXT("Main.Log.Copy");
	menuWidth += GetTextWidthInPixels(sLogCopy, 20);

	buttonCopy->setText( sLogCopy );

	QString sLogCopyAll = MAGIC_TEXT("Main.Log.CopyAll");
	menuWidth += GetTextWidthInPixels(sLogCopyAll, 20);

	buttonCopyAll->setText( sLogCopyAll );

	QString sLogClear = MAGIC_TEXT("Main.Log.Clear");
	menuWidth += GetTextWidthInPixels(sLogClear, 20);

	buttonClear->setText( sLogClear );

	QString sLogClose = MAGIC_TEXT("Main.Log.Close");
	menuWidth += GetTextWidthInPixels(sLogClose, 20);

	buttonClose->setText( sLogClose );

	// Okay.
	RecalculateWindowSize(logWidget, menuWidth, 450, 150);
}

void TxdLog::show( void )
{
	if (!positioned)
	{
		QPoint mainWindowPosn = parent->pos();
		QSize mainWindowSize = parent->size();
		logWidget->move(mainWindowPosn.x() + 15, mainWindowPosn.y() + mainWindowSize.height() - 200 - 15);
		positioned = true;
	}

	if (!logWidget->isVisible()) // not fure
		logWidget->show();
}

void TxdLog::hide( void )
{
	logWidget->hide();
}

QByteArray TxdLog::saveGeometry( void )
{
	return this->logWidget->saveGeometry();
}

bool TxdLog::restoreGeometry( const QByteArray& buf )
{
	bool restored = this->logWidget->restoreGeometry( buf );

	if ( restored )
	{
		this->positioned = true;
	}

	return restored;
}

void TxdLog::showError( QString msg )
{
	addLogMessage(msg, LOGMSG_ERROR);
	if (!logWidget->isVisible()) // not fure
		logWidget->show();
}

void TxdLog::addLogMessage( QString msg, eLogMessageType msgType )
{
	QPixmap *pixmap;
	switch (msgType)
	{
	case LOGMSG_WARNING:
		pixmap = &picWarning;
		break;
	case LOGMSG_ERROR:
		pixmap = &picError;
		break;
	default:
		pixmap = &picInfo;
	}

	QListWidgetItem *item = new QListWidgetItem;
	listWidget->insertItem(0, item);
	listWidget->setItemWidget(item, new LogItemWidget(msgType, *pixmap, msg));
	item->setSizeHint(QSize(listWidget->sizeHintForColumn(0), listWidget->sizeHintForRow(0)));
}

void TxdLog::clearLog( void )
{
	this->listWidget->clear();
}

void TxdLog::saveLog( QString fileName )
{
	// Try to open a file handle.
	QFile file(fileName);
	if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		QTextStream out(&file);
		out.setCodec("UTF-8");
		out.setGenerateByteOrderMark(true);

		// Put version.
		time_t currentTime = time( NULL );

		char timeBuf[ 1024 ];

		std::strftime( timeBuf, sizeof( timeBuf ), "%A %c", localtime( &currentTime ) );

		out <<
			"Magic.TXD generated log file on " << timeBuf << "\n" <<
			"compiled on " __DATE__ " version: " MTXD_VERSION_STRING << "\n";

		// Go through all log rows and print them.
		int numRows = this->listWidget->count();
		for (int n = numRows - 1; n >= 0; n--)
		{
			// Get message information.
			QListWidgetItem *logItem = (QListWidgetItem*)(this->listWidget->item(n));
			if (logItem)
			{
				LogItemWidget *logWidget = (LogItemWidget *)this->listWidget->itemWidget(logItem);
				out << getLogItemLine(logWidget);
			}
		}
	}
	else
	{
		// TODO: display reason why failed.
	}
}