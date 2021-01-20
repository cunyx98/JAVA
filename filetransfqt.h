#include <QtWidgets/QMainWindow>
#include "ui_filetransfqt.h"
#include <QMessageBox>
#include <QProgressBar> 
#include <QTextEdit>
#include <QTableWidget>
#include <QFileDialog>
#include <QThread>
#include <QMutex> 
#include <QMetaType> 
#include <QLabel>
#include <QFile>
#include <QDir>
#include <QDomDocument>
#include <QDateTime>
#include <QTimer>
#include <QMouseEvent>
#include <QPoint>
#include <QToolTip>
#include <QMimeData>
#include <QCloseEvent>

#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QJsonArray>

#include <QMenu>
#include <QSystemTrayIcon>  //托盘类
#include <QDesktopServices> //桌面事件类
#include <QAction>

#include <QApplication>
#include <QPropertyAnimation>
#include <QDesktopWidget>

#include <QTreeWidget>
#include <QStringList>

#include "pthread.h"
#include "setting.h"
#include "login.h"
#include "options.h"
#include "common.h"

#define ROWCOUNT 7     // tablewidget列数
#define ONEROWCOUNT 4  // completetablewidget列数

Q_DECLARE_METATYPE(Data*)
Q_DECLARE_METATYPE(Data)



enum eColName {
	ECOL_FILENAME,   // 文件名
	ECOL_TIME,		 // 完成时间
	ECOL_PRO,		 // 进度条
	ECOL_STATUS,	 // 状态
	ECOL_SPEED,      // 速率
	ECOL_COMPLETED,  // 完成大小
	ECOL_SIZE,		 // 文件大小
	ECOL_OTHERSIZE = ECOL_PRO,   // 完成表格的大小列
	ECOL_PATH = ECOL_STATUS,
};


enum RecordedObject {
	E_TABLEWIDGET,              // 传输列表
	E_COMPLETETABLEWIDGET		// 完成列表
};


class filetransfqt : public QMainWindow {
   
	Q_OBJECT

public:
    filetransfqt(QWidget *parent = Q_NULLPTR);
	~filetransfqt();
	
private:
	static int uploadInfoCb(void* obj = nullptr, string filepath = "" , string estimateTime = "", 
				double speed = 0, double progress = 0 ,long long nowSize = 0, long long localSize = 0);
	static int rece(void* obj,std::string& url);

	void outgivingInfo(void* obj ,string filepath , string estimateTime,
				double speed, double progress ,long long nowSize, long long localSize);
		
	void dealFailedTasks(void* obj, std::string& url);

	virtual void dragEnterEvent(QDragEnterEvent *e) override;
    virtual void dropEvent(QDropEvent *e) override;
	void closeEvent(QCloseEvent *event);
	void hideEvent(QHideEvent* event);
	void mousePressEvent(QMouseEvent* event);
	bool eventFilter(QObject* obj, QEvent* e);

	bool logOut(QString&, QString&);
	bool getJsonOfLogout(QString&);
	bool getJsonOfUpBack(QString&);
	
	bool fileTypeFilter(QString&);
	bool insertRowInfo(QFileInfo&,QString,int);
	bool recoverInsertRowInfo(Data&, int);
	QFileInfoList getAllFileList(QString&);
	
	void trayInit();
	void initOthing();
	
	void clearlistitem();
	void deleteInfo();
	void deleteInfo(int);
	void deleteData(int);
	void deleteInfo();
	void deleteInfo(int);
	void deleteData(int)
	void deleteInfo(int, int, QString);
	void deleteData(int, QString);

	QString getName(QString&);
	int getRowOfFilename(string&, int, bool flag = true);
	
	bool getDirectoryTree();
	void addChildNode(QTreeWidgetItem*, QString&, QString&, QString&);
	void analysisJson(QJsonValue&);
	void createDirTree(int, int, QString);
	void changeDir(QString&,QString&);
	QString defaultDir();
	QTreeWidgetItem* addTreeRoot(QString& root, QString& pid);
	QTreeWidgetItem* addTreeNode(QTreeWidgetItem* parent,QString& node, QString& pid);
	
	void recordList(QString&,RecordedObject);
	void showRecordList(QString&,RecordedObject);
	
	void statisticsTotalRate();

	// transfercompletepage
	void initCompleteTbwidget();
	void insertInfoIntoCompleteTbwidget(Data&, bool flag = false);
	void exitClear();

private slots:
	// mainpage
	void slots_upload_check();
	void slots_delete_check();
	void slots_openfile_check();
	void slots_openfolder_check();
	void slots_clearlist_check();
	void slots_start_check();
	void slots_stop_check();
	void slots_setup_check();

	void slots_receiveAdd(int);
	void slots_receiveRetry();

	void slots_receiveValue(double, string);
	void slots_receiveSpeed(double,string);
	void slots_receiveSize(double, double, string);
	void slots_receiveTime(string, string);

	void slots_buttonRelease();
	void slots_buttonDblClick();
	void slots_showToolTip(QModelIndex index);
	
	void slots_receiveAutomaticDel(Data*,QString&);

	void slots_receiveSuccessFlag(Data *);
	void slots_receivePromptInfo(Data*);

	void slots_iconActivated(QSystemTrayIcon::ActivationReason ireason);

	void slots_showMainpage();
	void slots_showTransferCompletepage();

	void slots_onedir_check(QTreeWidgetItem*, int);

	void slots_trayIconClose();
	void slots_sortByColumn(int);


	// transfercompletepage
	void slots_clearAllrecord();
	

signals:
	void signal_sendCompleted(Data*);
	void signal_sendAdd(int);

	void signal_sendData(Data*);

	void signal_sendValue(double, string);
	void signal_sendSpeed(double,string);
	void signal_sendSize(double, double, string);
	void signal_sendTime(string, string);
	
	void signal_buttonRelease();
	void signal_buttonDblClick();

	void signal_sendTreeNode(int, int, QString);
	 
	void signal_sendDataToOptionDialog(Data* ,QString);

private:
	Ui::filetransfqtClass ui;
	FtpProtocol *fp=nullptr;
	Pthread *thread=nullptr;
	FlagPthread *fthread = nullptr;
	FileSizethread *fsthread = nullptr;
	QString host;	
	QString user;
	QString password;
	QString path;             // 根目录
	QString protocal;
	QString token;
	QString username;
	QString name;
	int port = 0;

	QString childpath="";        // 用户选择的子目录路径
	QString childpathId="";      // 子目录路径id

	bool startThreadFlag = false;
	QSettings* configIni  = new QSettings("para.ini", QSettings::IniFormat);

	QVector<QString> fileType;  // 文件类型数组

	QLabel* speedlb=nullptr;
	QLabel* numlb=nullptr;

private:
	QMenu* trayMenu=nullptr;               // 托盘菜单
	QSystemTrayIcon* trayIcon=nullptr;     // 托盘图标添加成员
	QAction* minimizeAction=nullptr;       // 最小化动作
	QAction* maximizeAction=nullptr;       // 最大化动作
	QAction* quitAction=nullptr;           // 退出动作
	QAction* restoreAction=nullptr;        // 显示主窗口


}; 



// sort by clonum
class TableWidgetItem : public QTableWidgetItem
{
public:
	TableWidgetItem(const QString& text) :
		QTableWidgetItem(text)
	{
	}
public:

	bool operator <(const QTableWidgetItem& other) const{
		QString B = "B";
		QString MB = "MB";
		QString KB = "KB";
		QString GB = "GB";
		int bit = 1024;

		int pos = text().indexOf(" ");
		QString lnum = text().mid(0, pos);
		QString lunit = text().mid(pos+1);

		long long ltext=0;
		if (lunit == B) {
			ltext = lnum.toDouble()*100;
		}
		else if (lunit == KB) {
			ltext = lnum.toDouble()*100;
			ltext = ltext * bit;
		}
		else if (lunit == MB) {
			ltext = lnum.toDouble()*100;
			ltext = ltext* bit * bit;
		}
		else if (lunit == GB) {
			ltext = lnum.toDouble()*100;
			ltext = ltext * bit * bit * bit;
		}

		int opos = other.text().indexOf(" ");
		QString onum = other.text().mid(0, opos);
		QString ounit = other.text().mid(opos+1);

		long long lotext=0;
		if (ounit == B) {
			lotext = onum.toDouble()*100;
		}
		else if (ounit == KB) {
			lotext = onum.toDouble()*100;
			lotext = lotext * bit;
		}
		else if (ounit == MB) {
			lotext = onum.toDouble()*100;
			lotext = lotext * bit * bit;
		}
		else if (ounit == GB) {
			lotext = onum.toDouble()*100;
			lotext = lotext * bit * bit * bit;
		}

		return ltext < lotext;
	}
};
