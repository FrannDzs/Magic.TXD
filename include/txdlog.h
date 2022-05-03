/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/txdlog.h
*  PURPOSE:     Header of the main editor log.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

#include <QtCore/qlist.h>
#include <QtWidgets/qwidget.h>
#include <QtCore/qstring.h>
#include <QtWidgets/qlistview.h>
#include <QtWidgets/qdialog.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qscrollbar.h>
#include <QtWidgets/qfiledialog.h>
#include <QtCore/qobject.h>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/qlistwidget.h>
#include <QtWidgets/QListWidgetItem>
#include <QtWidgets/QApplication>
#include <QtGui/QClipboard>

#include "defs.h"
#include "languages.h"

#include <ctime>

enum eLogMessageType
{
	LOGMSG_INFO,
	LOGMSG_WARNING,
	LOGMSG_ERROR
};

class MainWindow;

class LogItemWidget : public QWidget
{
	eLogMessageType msgType;

public:
	QLabel *textLabel;

	LogItemWidget(eLogMessageType MessageType, QPixmap &pixmap, QString message) : QWidget()
	{
		msgType = MessageType;
		QLabel *iconLabel = new QLabel();
		iconLabel->setPixmap(pixmap);
		iconLabel->setFixedSize(20, 20);
		textLabel = new QLabel(message);
		textLabel->setObjectName("logText");
		QHBoxLayout *layout = new QHBoxLayout();
		layout->setContentsMargins(2, 2, 2, 2);
		layout->addSpacing(8);
		layout->addWidget(iconLabel);
		layout->addWidget(textLabel);
		this->setLayout(layout);
	}

	eLogMessageType getMessageType()
	{
		return this->msgType;
	}

	QString getText()
	{
		return this->textLabel->text();
	}
};

class TxdLog : public QObject, public magicTextLocalizationItem
{
public:
	TxdLog(MainWindow *mainWnd, QString AppPath, QWidget *ParentWidget);
	~TxdLog( void );

	void show();
	void hide();

	QByteArray saveGeometry( void );
	bool restoreGeometry( const QByteArray& buf );

	void showError(QString msg);

	void addLogMessage(QString msg, eLogMessageType msgType = LOGMSG_INFO);
	void clearLog(void);

	void updateContent( MainWindow *mainWnd ) override;

private:
	QString getLogItemLine(LogItemWidget *itemWidget)
	{
		QString str = "*** [\"";
		QString spacing = "\n          ";

		QString label;
		switch (itemWidget->getMessageType())
		{
		case LOGMSG_ERROR:
			label = MAGIC_TEXT("Main.Log.ErrorLabel");
			break;
		case LOGMSG_WARNING:
			label = MAGIC_TEXT("Main.Log.WarningLabel");
			break;
		default:
			label = MAGIC_TEXT("Main.Log.InfoLabel");
			break;
		}

		spacing += QString(" ").repeated(label.size());
		str += label + "\"]: ";

		QString message = itemWidget->textLabel->text();
		message.replace("\n", spacing);
		str += message;
		str += "\n";
		return str;
	}

	static void strToClipboard(const QString &s)
	{
		QClipboard *clipboard = QApplication::clipboard();
		clipboard->setText(s);
	}

public:
	void saveLog(QString fileName);

public slots:
	void onLogSaveRequest()
	{
		QString saveFileName = QFileDialog::getSaveFileName(parent, MAGIC_TEXT("Main.Log.SaveAs"), QString(), "Log File (*.txt)");

		if (saveFileName.length() != 0)
		{
			saveLog(saveFileName);
		}
	}

	void onLogClearRequest()
	{
		this->clearLog();
	}

	void onWindowHideRequest()
	{
		this->hide();
	}

	void onCopyLogLinesRequest()
	{
		QList<QListWidgetItem*> selection = listWidget->selectedItems();
		QList<QListWidgetItem *> reversed;
		reversed.reserve(selection.size());
		std::reverse_copy(selection.begin(), selection.end(), std::back_inserter(reversed));
		QString str;
		foreach(QListWidgetItem *item, reversed)
		{
			LogItemWidget *logWidget = (LogItemWidget *)this->listWidget->itemWidget(item);
			str += getLogItemLine(logWidget);
		}
		strToClipboard(str);
	}

	void onCopyAllLogLinesRequest()
	{
		QString str;
		int numRows = this->listWidget->count();
		for (int n = numRows - 1; n >= 0; n--)
		{
			// Get message information.
			QListWidgetItem *logItem = (QListWidgetItem*)(this->listWidget->item(n));
			if (logItem)
			{
				LogItemWidget *logWidget = (LogItemWidget *)this->listWidget->itemWidget(logItem);
				str += getLogItemLine(logWidget);
			}
		}
		strToClipboard(str);
	}

private:
	MainWindow *mainWnd;

	QPushButton *buttonSave;
	QPushButton *buttonCopy;
	QPushButton *buttonCopyAll;
	QPushButton *buttonClear;
	QPushButton *buttonClose;

	QWidget *parent;
	QWidget *logWidget;
	QListWidget *listWidget;

	bool positioned;

	QPixmap picWarning;
	QPixmap picError;
	QPixmap picInfo;
};
