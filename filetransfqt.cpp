#include "filetransfqt.h"

bool CompareDescending(const int& a, const int& b) {   // 降序
	return a > b;
}

bool CompareAscending(const int& a, const int& b) {   // 升序
	return a < b;
}


filetransfqt::filetransfqt(QWidget* parent)
	: QMainWindow(parent)
{
	MAXRUNNING = configIni->value("SETTINGS/tasksnum").toInt();
	ui.setupUi(this);
	qRegisterMetaType<std::string>("string");								 // 注册string 类型数据  槽函数需要注册
	qRegisterMetaType<Data>("Data &");										 // 注册Data 引用类型
	qRegisterMetaType<Data*>("Data *");
	qRegisterMetaType<QVector<int>>("QVector<int>");
	qRegisterMetaType<QString>("QString&");
	//setFixedSize(this->width(), this->height());							 // 不允许最大化
	ui.tablewidget->setColumnCount(ROWCOUNT);								 // 设置列										
	QStringList headers;                                                     // 设置标头
	headers << "文件名称" << "完成时间" << "进度" << "状态" << "速率" << "传输大小" << "文件大小";
	ui.tablewidget->setHorizontalHeaderLabels(headers);
	ui.tablewidget->setStyleSheet("selection-background-color:rgb(193,210,240)");        // 设置选中单元格颜色
	ui.tablewidget->setMinimumSize(600, 400);
	ui.tablewidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);  // 自适应宽度  4个可选项
	ui.tablewidget->horizontalHeader()->setMinimumHeight(40);                            // 表头高度
	ui.tablewidget->horizontalHeader()->setFont(QFont("Microsoft YaHei", 11));		     // 表头字体
	ui.tablewidget->setColumnWidth(ECOL_FILENAME, 220);								     // 设置第一列的宽度
	ui.tablewidget->setColumnWidth(ECOL_TIME, 150);
	ui.tablewidget->setColumnWidth(ECOL_PRO, 100);
	ui.tablewidget->setColumnWidth(ECOL_STATUS, 80);
	ui.tablewidget->setColumnWidth(ECOL_SPEED, 100);
	ui.tablewidget->setColumnWidth(ECOL_COMPLETED, 140);
	ui.tablewidget->setColumnWidth(ECOL_SIZE, 100);
	ui.tablewidget->setEditTriggers(QAbstractItemView::NoEditTriggers);					 // 设置单元格不可编辑
	ui.tablewidget->verticalHeader()->hide();                                            // verticalHeader  竖直表头  horizontalHeader  水平表头   // 隐藏行号
	ui.tablewidget->setShowGrid(false);													 // 隐藏网格线
	ui.tablewidget->setSelectionBehavior(QAbstractItemView::SelectRows);				 // 设置选择行为，以行为单位
	ui.tablewidget->setSelectionMode(QAbstractItemView::SingleSelection);				 // 设置选择模式，选择单行
	ui.tablewidget->setSelectionMode(QAbstractItemView::ExtendedSelection);				 // 设置一次选择多行
	ui.tablewidget->setFocusPolicy(Qt::NoFocus);										 // 隐藏选中框
	ui.tablewidget->horizontalHeader()->setHighlightSections(false);					 // 取消表头的在选中单元格时的高亮状态
	setAcceptDrops(true);
	ui.tablewidget->viewport()->installEventFilter(this);								 // 安装事件过滤器
	ui.tablewidget->setMouseTracking(true);												 // 开启鼠标捕获
	trayInit();																			 // 托盘及关闭事件初始化
	
	initCompleteTbwidget();																 // 完成界面的初始化


	fp = new FtpProtocol();
	if (!fp) {
		return;
	}

	initOthing();                                                            // 相关信息的初始化

	thread = new Pthread(fp);
	if (!thread) {
		return;
	}
	fthread = new FlagPthread(fp);
	if (!fthread) {
		return;
	}

	fsthread = new FileSizethread(fp);
	if (!fsthread) {
		return;
	}

	// 按键初始化
	ui.cleartablebutton->setEnabled(false);
	ui.deletebutton->setEnabled(false);
	ui.startbutton->setEnabled(false);
	ui.stopbutton->setEnabled(false);
	ui.uploadbutton->setEnabled(false);

	// 显示上次的记录
	showRecordList(username,E_TABLEWIDGET);
	showRecordList(username,E_COMPLETETABLEWIDGET);
	// 初始化最近选择的目录
	childpath = configIni->value("LastPath/dirpath").toString();
	childpathId = configIni->value("LastPath/dirpathId").toString();

	// 切换页
	ui.stackwidget->setCurrentWidget(ui.mainpage);    // 名称
	ui.transfercompletebutton->setStyleSheet("QPushButton{ font: 15px \"Microsoft YaHei\"; color:rgb(0, 0, 0)}");
	ui.mainbutton->setStyleSheet("QPushButton{ font: 15px \"Microsoft YaHei\"; color:rgb(6, 168, 255)}");
	connect(ui.mainbutton, SIGNAL(clicked()), this, SLOT(slots_showMainpage()));
	connect(ui.transfercompletebutton, SIGNAL(clicked()), this, SLOT(slots_showTransferCompletepage()));


	// 目录树窗口初始化
	ui.dirtreewidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
	ui.dirtreewidget->setColumnCount(2);
	ui.dirtreewidget->setHeaderLabels(QStringList() << "媒资目录" << "目录id");
	ui.dirtreewidget->setColumnHidden(1, true);
	getDirectoryTree();
	ui.dirtreewidget->expandAll();
	connect(ui.dirtreewidget, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(slots_onedir_check(QTreeWidgetItem*, int)));
 	QString dir = defaultDir();
	ui.statusBar->showMessage("当前目录: " + dir);

	// 底部状态栏
	ui.statusBar->setSizeGripEnabled(false);
	numlb = new QLabel(QString::number(0) + " B/s");
	ui.statusBar->addPermanentWidget(numlb, 0);
	speedlb = new QLabel;
	QString text = "总速率 ";
	speedlb->setText(text);
	QFont ft;
	ft.setPointSize(9);
	speedlb->setFont(ft);
	ui.statusBar->addPermanentWidget(speedlb, 0);
	QLabel* lb = new QLabel("    ");
	ui.statusBar->addPermanentWidget(lb, 0);

	// 列排序
	connect(ui.tablewidget->horizontalHeader(), SIGNAL(sectionClicked(int)), this, SLOT(slots_sortByColumn(int)));

	// 按钮点击
	connect(ui.uploadbutton, SIGNAL(clicked()), this, SLOT(slots_upload_check()));
	connect(ui.openfilebutton, SIGNAL(clicked()), this, SLOT(slots_openfile_check()));
	connect(ui.openfolderbutton, SIGNAL(clicked()), this, SLOT(slots_openfolder_check()));
	connect(ui.deletebutton, SIGNAL(clicked()), this, SLOT(slots_delete_check()));
	connect(ui.cleartablebutton, SIGNAL(clicked()), this, SLOT(slots_clearlist_check()));
	connect(ui.startbutton, SIGNAL(clicked()), this, SLOT(slots_start_check()));
	connect(ui.stopbutton, SIGNAL(clicked()), this, SLOT(slots_stop_check()));
	connect(ui.settingbutton, SIGNAL(clicked()), this, SLOT(slots_setup_check()));

	// 鼠标事件
	connect(this, SIGNAL(signal_buttonRelease()), this, SLOT(slots_buttonRelease()));
	connect(this, SIGNAL(signal_buttonDblClick()), this, SLOT(slots_buttonDblClick()));
	connect(ui.tablewidget, SIGNAL(entered(QModelIndex)), this, SLOT(slots_showToolTip(QModelIndex)));

	// 回调通知线程相关
	connect(this, SIGNAL(signal_sendCompleted(Data*)), fthread, SLOT(slots_receiveCompleted(Data*)), Qt::QueuedConnection);
	connect(fthread, SIGNAL(signal_sendAutomaticDel(Data*, QString&)), this, SLOT(slots_receiveAutomaticDel(Data*, QString&)), Qt::QueuedConnection);

	// 上传线程相关
	connect(this, SIGNAL(signal_sendData(Data*)), fsthread, SLOT(slots_receiveData(Data*)), Qt::QueuedConnection);

	connect(fsthread, SIGNAL(signal_sendPromptInfo(Data*)), this, SLOT(slots_receivePromptInfo(Data*)), Qt::BlockingQueuedConnection);
	connect(fsthread, SIGNAL(signal_sendSuccessFlag(Data*)), this, SLOT(slots_receiveSuccessFlag(Data*)), Qt::QueuedConnection);

	// 主窗口相关
	connect(this, SIGNAL(signal_sendTime(string, string)), this, SLOT(slots_receiveTime(string, string)), Qt::QueuedConnection);
	connect(this, SIGNAL(signal_sendValue(double, string)), this, SLOT(slots_receiveValue(double, string)), Qt::QueuedConnection);
	connect(this, SIGNAL(signal_sendSize(double, double, string)), this, SLOT(slots_receiveSize(double, double, string)), Qt::QueuedConnection);
	connect(this, SIGNAL(signal_sendSpeed(double, string)), this, SLOT(slots_receiveSpeed(double, string)), Qt::QueuedConnection);
	connect(this, SIGNAL(signal_sendAdd(int)), this, SLOT(slots_receiveAdd(int)), Qt::QueuedConnection);

	// 上传线程相关(重试)
	connect(thread, SIGNAL(signal_sendRetry()), this, SLOT(slots_receiveRetry()), Qt::QueuedConnection);
	
}

filetransfqt::~filetransfqt(){
	if (fp) {
		delete fp;
		fp = nullptr;
	}

	if (fsthread) {
		fsthread->quit();
		fsthread->wait(1);
		delete fsthread;
		fsthread = nullptr;
	}
	if (fthread) {
		fthread->quit();
		fthread->wait(1);
		delete fthread;
		fthread = nullptr;
	}
	if (thread) {
		thread->quit();
		thread->wait(1);
		delete thread;
		thread = nullptr;
	}
	
	if (configIni)	{
		delete configIni;
	}

	if (speedlb) {
		delete speedlb;
	}
	if (numlb) {
		delete numlb;
	}
}

// 上传任务
void filetransfqt::slots_upload_check() {
	thread->UnExitTh();   // 如果有清空
	int rowCount = ui.tablewidget->rowCount();
	if (!rowCount) {
		showHintLabel(this, "选择要上传的文件",1500, 150, 60, "15px");
		return;
	}

	int controlQuantity = 0;
	for (int row = 0; row < rowCount; ++row) {
		QTableWidgetItem* it = ui.tablewidget->item(row, ECOL_FILENAME);
		if (!it) {
			return;
		}
		QVariant var = it->data(1);
		Data* data = (Data*)var.value<void*>();
		if (!data) {
			return;
		}
		if (data->_status == TRANSFERING || 
			data->_status == FINSHED ||
			data->_status == WAITING ) {
			continue;
		}

		if (controlQuantity >= MAXRUNNING ) {
			break;
		}
		controlQuantity++;

		data->_status_prev = data->_status;
		data->_status = WAITING;
		ui.tablewidget->item(row, ECOL_SPEED)->setText("-- --");
		emit this->signal_sendData(data);
		LOG_INFO("%s calcu remote size", qstrTOstr(data->_filepath).c_str());

		ui.tablewidget->item(row, ECOL_STATUS)->setText(statusName[data->_status]);
		
	}
	fp->setPFunc(filetransfqt::uploadInfoCb, this, filetransfqt::rece);
	ui.uploadbutton->setEnabled(false);
}

// 开始任务
void filetransfqt::slots_start_check() {
	thread->UnExitTh();   // 如果有清空
	QList<QTableWidgetItem*> selection = ui.tablewidget->selectedItems();
	if (selection.isEmpty())
		return;

	vector<int> itemIndex;								                          
	for (int i = 0; i < selection.size(); ++i) {
		itemIndex.push_back(selection.at(i)->row());                              // 存储
	}
	sort(itemIndex.begin(), itemIndex.end(), CompareAscending);	                  // 降序排序
	itemIndex.erase(unique(itemIndex.begin(), itemIndex.end()), itemIndex.end()); // 去重
	
	// 开始按钮关注暂停、未开始两种状态
	int controlQuantity = 0;         
	for (int i = 0; i < itemIndex.size(); ++i) {
		QTableWidgetItem* it = ui.tablewidget->item(itemIndex[i], ECOL_FILENAME);
		if (!it) {
			return;
		}
		QVariant var = it->data(1);
		Data* data = (Data*)var.value<void*>();
		if (!data) {
			return;
		}
	
		if (data->_status == STOP || data->_status == NOTSTART || data->_status == FAIL) {
			if (controlQuantity >= MAXRUNNING) {
				continue;
			}
			controlQuantity++;

			emit this->signal_sendData(data);
			LOG_INFO("%s start!!!", qstrTOstr(data->_filepath).c_str());

			data->_status_prev = data->_status;
			data->_status = WAITING;
			data->_bstatu = !data->_bstatu;           // 对当前状态取反
			ui.tablewidget->item(itemIndex[i], ECOL_STATUS)->setText(statusName[data->_status]);
			ui.tablewidget->item(itemIndex[i], ECOL_SPEED)->setText("-- b/s");
			ui.tablewidget->item(itemIndex[i], ECOL_TIME)->setText("");

			ui.startbutton->setEnabled(false);
			ui.stopbutton->setEnabled(true);

#ifdef _DEBUG
			qDebug() << "[LOG]" << qstrTOstr(data->_filepath).c_str() << "start!!!" << endl;
#endif
		}
		else if (data->_status == FINSHED || data->_status == TRANSFERING 
							|| data->_status == WAITING ) {
			continue;
		}
	}
	fp->setPFunc(filetransfqt::uploadInfoCb, this, filetransfqt::rece);
}

// 暂停任务
void filetransfqt::slots_stop_check(){
	QList<QTableWidgetItem*> selection = ui.tablewidget->selectedItems();
	if (selection.isEmpty()){
		return;
	}

	vector<int> itemIndex;															
	for (int i = 0; i < selection.size(); ++i) {
		itemIndex.push_back(selection.at(i)->row());								 // 存储行号
	}

	sort(itemIndex.begin(), itemIndex.end(), CompareAscending);						 // 降序排序
	itemIndex.erase(unique(itemIndex.begin(), itemIndex.end()), itemIndex.end());    // 去重
	
	// 暂停按钮只关注传输中、等待传输两种状态
	for (int i = 0; i < itemIndex.size(); ++i) {
		QTableWidgetItem* it = ui.tablewidget->item(itemIndex[i], ECOL_FILENAME);
		if (!it) {
			return;
		}
		QVariant var = it->data(1);
		Data* data = (Data*)var.value<void*>();
		if (!data) {
			return;
		}

		if (data->_status == TRANSFERING || data->_status == WAITING) {	
			data->_status_prev = data->_status;
			data->_status = STOP;
			data->_bstatu = !data->_bstatu;              // 对当前状态取反
			ui.tablewidget->item(itemIndex[i], ECOL_STATUS)->setText(statusName[data->_status]);

			fp->removeEasyhandle(data->_info);
			LOG_INFO("%s stoped!!!", qstrTOstr(data->_filepath).c_str());

			if (data->_status_prev == TRANSFERING ) {     // 这里过滤  WAITING并没有让TRANSFERINGCOUNT+1，所以过滤
				if (TRANSFERINGCOUNT > 0) {
					_mutex.lock();
					TRANSFERINGCOUNT--;
					_mutex.unlock();
				}
				if (TRANSFERINGCOUNT == 0) {
				
					ui.uploadbutton->setEnabled(true);
				}
				qDebug() << "1 TRANSFERINGCOUNT--"<<TRANSFERINGCOUNT;
			}
			
			ui.tablewidget->item(itemIndex[i], ECOL_SPEED)->setText("");
			ui.tablewidget->item(itemIndex[i], ECOL_TIME)->setText("");
			ui.startbutton->setEnabled(true);
			ui.stopbutton->setEnabled(false);
#ifdef _DEBUG
			qDebug() << "[LOG]" << qstrTOstr(data->_filepath).c_str() << "stoped!!!" << endl;
#endif
		}
		else if(data->_status == STOP || data->_status == FINSHED || data->_status == NOTSTART) {
			continue;
		}
	}
}

// 删除选中的行(删除一行/多行,任意连续或不连续)
void filetransfqt::slots_delete_check(){
	QList<QTableWidgetItem *> selection = ui.tablewidget->selectedItems();
	if (selection.isEmpty())
		return;

	vector<int> itemIndex;								                          // 存储要删除的行号
	for (int i = 0; i < selection.size(); ++i) {
		itemIndex.push_back(selection.at(i)->row());                              // 存储
	}

	sort(itemIndex.begin(), itemIndex.end(), CompareDescending);	              // 排序
	itemIndex.erase(unique(itemIndex.begin(), itemIndex.end()), itemIndex.end()); // 去重

	for (int i = 0; i < itemIndex.size(); ++i) {
		QTableWidgetItem* it = ui.tablewidget->item(itemIndex[i], ECOL_FILENAME);
		if (!it) {
			return;
		}
		QVariant var = it->data(1);
		Data* data = (Data*)var.value<void*>();
		if (!data) {
			return;
		}

		// 状态为传输中或等待的任务，需先暂停
		if (data->_status == TRANSFERING || data->_status == WAITING) {
			data->_status = STOP;

			fp->removeEasyhandle(data->_info);
			fp->deleteInfo(data->_info);
		}

		if (data->_status == FINSHED || data->_status == NOTSTART) {       
			fp->deleteInfo(data->_info);
		}

		deleteData(itemIndex[i]);
	}

	if (ui.tablewidget->rowCount() == 0) {
		ui.cleartablebutton->setEnabled(false);
		ui.deletebutton->setEnabled(false);
		ui.startbutton->setEnabled(false);
		ui.stopbutton->setEnabled(false);
		ui.uploadbutton->setEnabled(false);
	}
	else {
		ui.startbutton->setEnabled(false);
		ui.stopbutton->setEnabled(false);
	}
}

// 清空任务列表
void filetransfqt::slots_clearlist_check() {
	for (int row = ui.tablewidget->rowCount() - 1; row >= 0; row--) {
		QTableWidgetItem* it = ui.tablewidget->item(row, ECOL_FILENAME);
		if (!it) {
			return;
		}
		QVariant var = it->data(1);
		Data* data = (Data*)var.value<void*>();
		if (!data) {
			return;
		}

		// 暂停正在传输的
		if (data->_status == TRANSFERING) {
			data->_status = STOP;
			fp->removeEasyhandle(data->_info);
		}

		fp->deleteInfo(data->_info);
		deleteData(row);
	}
	
	_mutex.lock();
	TRANSFERINGCOUNT = 0;
	_mutex.unlock();
	qDebug() << "clear" << TRANSFERINGCOUNT;
	LOG_INFO("clear list");

	ui.cleartablebutton->setEnabled(false);
	ui.startbutton->setEnabled(false);
	ui.deletebutton->setEnabled(false);
	ui.stopbutton->setEnabled(false);
	ui.uploadbutton->setEnabled(false);

	if (thread->isRunning()) {
		thread->ExitTh(); // 为了线程不再空转
	}
}

// 打开文件(一个或多个)
void filetransfqt::slots_openfile_check() {
	// 选择目录过滤
	if (childpath.size()==0) {
		QString text = "请选择要上传的目录!!!";
		showHintLabel(this, text, 1500, 150, 60, "16px");
		return;
	}

	QString sPreFile = configIni->value("open_file/path", QVariant("C://")).toString();
	QStringList fileInfoList = QFileDialog::getOpenFileNames(this, tr("选择文件"), sPreFile, tr("全部文件(*)"));
	
	if (!fileInfoList.size())
		return;

	sPreFile = QFileInfo(fileInfoList[0]).path();
	configIni->setValue("open_file/path", QVariant(sPreFile));

	bool bret = false;
	bool ret = false;
	for (int i = 0; i < fileInfoList.size(); ++i) {
		int rowCount = ui.tablewidget->rowCount();
		QFileInfo fileInfo = QFileInfo(fileInfoList[i]);
		bret = insertRowInfo(fileInfo, "", rowCount);
		if (bret) {
			ret = true;
		}
	}

	if (ret) {
		ui.cleartablebutton->setEnabled(true);
		ui.deletebutton->setEnabled(true);
		ui.uploadbutton->setEnabled(true);
	}
}

// 打开文件夹
void filetransfqt::slots_openfolder_check() {
	// 选择目录过滤
	if (childpath.size() == 0) {
		QString text = "请选择要上传的目录!!!";
		showHintLabel(this, text, 1500, 150, 60, "16px");
		return;
	}

	QString sPreFolder = configIni->value("open_folder/path", QVariant("C://")).toString();
	QString fileDirPath = QFileDialog::getExistingDirectory(this,tr("选择文件夹"),sPreFolder,QFileDialog::ShowDirsOnly);
																
	if (fileDirPath.isEmpty()) {
		return;
	}
	configIni->setValue("open_folder/path", QVariant(fileDirPath));

	bool bret = false;
	bool ret = false;
    QFileInfoList fileInfoList = getAllFileList(fileDirPath);
	for (QFileInfo fileInfo : fileInfoList) {
		int rowCount = ui.tablewidget->rowCount();
		bret = insertRowInfo(fileInfo, fileDirPath, rowCount);
		if (bret) {
			ret = true;
		}
	}

	if (ret) {
		ui.cleartablebutton->setEnabled(true);
		ui.deletebutton->setEnabled(true);
		ui.uploadbutton->setEnabled(true);
	}
}

// 设置
void filetransfqt::slots_setup_check(){
	setting set;
	set.exec();
	MAXRUNNING = set.getTasksnum();	
}

// 自动添加任务
void filetransfqt::slots_receiveAdd(int addnum){
	int tmp = addnum;
	int row = 0; 
	int rowCount = ui.tablewidget->rowCount();
	for (row = 0; row < rowCount; row++) {
		QTableWidgetItem* it = ui.tablewidget->item(row, ECOL_FILENAME);
		if (!it) {
			return;
		}
		QVariant var = it->data(1);
		Data* data = (Data*)var.value<void*>();
		if (!data) {
			return;
		}
		
		if (data->_status == TRANSFERING ||
			data->_status == FINSHED ||  
			data->_status == STOP ||
			data->_status == WAITING ||
			data->_status == FAIL) {
			continue;
		}
		else{
			emit this->signal_sendData(data);
			LOG_INFO("%s automatic add to list", qstrTOstr(data->_filepath).c_str());
			
			data->_status_prev = data->_status;
			data->_status = WAITING;
			ui.tablewidget->item(row, ECOL_STATUS)->setText(statusName[data->_status]);
			fp->setPFunc(filetransfqt::uploadInfoCb, this, filetransfqt::rece);
			if (--tmp == 0) {
				break;
			}
		}
	}
}

// 功能同 slots_clearlist_check();
void filetransfqt::clearlistitem(){
	for (int row = ui.tablewidget->rowCount() - 1; row >= 0; row--) {
		QTableWidgetItem* it = ui.tablewidget->item(row, ECOL_FILENAME);
		if (!it) {
			return;
		}
		QVariant var = it->data(1);
		Data* data = (Data*)var.value<void*>();
		if (!data) {
			return;
		}


		if (data->_status == TRANSFERING|| data->_status == WAITING) {
			data->_status = STOP;
			fp->removeEasyhandle(data->_info);
		}
	}

	LOG_INFO("clear list");

	ui.cleartablebutton->setEnabled(false);
	ui.startbutton->setEnabled(false);
	ui.deletebutton->setEnabled(false);
	ui.stopbutton->setEnabled(false);
	ui.uploadbutton->setEnabled(false);
}

// 系统托盘设置
void filetransfqt::trayInit(){
	trayIcon = new QSystemTrayIcon(this);

	trayIcon->setIcon(QIcon(QPixmap("./icon/widget/upload.ico")));
	trayIcon->setToolTip("HDupload");
//	QString title = "APP Message";
//	QString text = "HDupload start up";
	trayIcon->show();                                                            // 托盘图标显示在系统托盘上
//	trayIcon->showMessage(title, text, QSystemTrayIcon::NoIcon, 3000);
//	trayIcon->showMessage(title, text, QSystemTrayIcon::Information, 3000);

	//创建菜单项动作
	minimizeAction = new QAction(tr("隐藏到托盘"), this);
	//quitAction->setIcon(QIcon("./icon/menu/xxx.ico"));
	connect(minimizeAction, SIGNAL(triggered()), this, SLOT(hide()));
	maximizeAction = new QAction(tr("最大化窗口"), this);
	//quitAction->setIcon(QIcon("./icon/menu/xxx.ico"));
	connect(maximizeAction, SIGNAL(triggered()), this, SLOT(showMaximized()));
	restoreAction = new QAction(tr("显示主窗口"), this);
	connect(restoreAction, SIGNAL(triggered()), this, SLOT(showNormal()));
	quitAction = new QAction(tr("退出"), this);
	//quitAction->setIcon(QIcon("./icon/menu/xxx.ico"));
	connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
	connect(quitAction, SIGNAL(triggered()), this, SLOT(slots_trayIconClose()));   
	 
	//创建托盘菜单
	trayMenu = new QMenu(this);
	trayMenu->addAction(minimizeAction);
	trayMenu->addAction(maximizeAction);
	trayMenu->addAction(restoreAction);
	trayMenu->addSeparator();
	trayMenu->addAction(quitAction);
	trayIcon->setContextMenu(trayMenu);

	connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(slots_iconActivated(QSystemTrayIcon::ActivationReason)));
}

// 相关信息的初始化
void filetransfqt::initOthing(){
	host = configIni->value("data/host").toString();
	user = configIni->value("data/user").toString();
	password = configIni->value("data/password").toString();
	path = configIni->value("data/path").toString();
	protocal = configIni->value("data/protocal").toString();
	token = configIni->value("data/token").toString();    
	username = configIni->value("data/username").toString();
	name = configIni->value("data/name").toString();
	port = configIni->value("data/port").toInt();

	int size = configIni->beginReadArray("allowtype");
	for (int i = 0; i < size; i++) {
		configIni->setArrayIndex(i);
		fileType.push_back(configIni->value("type").toString());
	}
	configIni->endArray();

	fp->initOther(qstrTOstr(host), qstrTOstr(path), port, qstrTOstr(user), qstrTOstr(password), PASV);
	LOG_INFO("Initialization related information success");
}

// 删除data的info
void filetransfqt::deleteInfo(){
	for (int row = ui.tablewidget->rowCount() - 1; row >= 0; row--) {
		QTableWidgetItem* it = ui.tablewidget->item(row, ECOL_FILENAME);
		if (!it) {
			return;
		}
		QVariant var = it->data(1);
		Data* data = (Data*)var.value<void*>();
		if (!data) {
			return;
		}
		delete data->_info;

		deleteData(row);
	}
}

// 删除info
void filetransfqt::deleteInfo(int row){
	QTableWidgetItem* it = ui.tablewidget->item(row, ECOL_FILENAME);
	if (!it) {
		return;
	}
	QVariant var = it->data(1);
	Data* data = (Data*)var.value<void*>();
	if (!data) {
		return;
	}
	delete data->_info;
}

// 默认重命名时截取显示当前的名字
QString filetransfqt::getName(QString& s){
	int npos = s.lastIndexOf("/");
	int pos = s.lastIndexOf(".");
	return s.mid(npos + 1, pos);
}

// 目录树添加根节点
QTreeWidgetItem* filetransfqt::addTreeRoot(QString& root,QString& pid){
	QIcon icon;
	icon.addPixmap(QPixmap("./icon/dirtree/open.ico"), QIcon::Normal, QIcon::On);        //节点打开状态 
	icon.addPixmap(QPixmap("./icon/dirtree/close.ico"), QIcon::Normal, QIcon::Off);      //节点关闭状态　
	QTreeWidgetItem* item = new QTreeWidgetItem(QStringList() << root << pid);
	item->setIcon(0, icon);
	ui.dirtreewidget->addTopLevelItem(item);
	return item;
}

// 目录树根节点添加子节点
QTreeWidgetItem* filetransfqt::addTreeNode(QTreeWidgetItem* parent, QString& node,QString& pid){
	QIcon icon;
	icon.addPixmap(QPixmap("./icon/dirtree/open.ico"), QIcon::Normal, QIcon::On);        //节点打开状态 
	icon.addPixmap(QPixmap("./icon/dirtree/close.ico"), QIcon::Normal, QIcon::Off);      //节点关闭状态　
	QTreeWidgetItem* item = new QTreeWidgetItem(QStringList() << node << pid);
	item->setIcon(0, icon);
	parent->addChild(item);
	return item;
}

// 退出记录
void filetransfqt::recordList(QString& user, RecordedObject robj){
	DataList list;
	QDir qdir;
	QString dirpath = "./user/" + user;
	QDir checkdir = "./user/" + user;
	
	if (!checkdir.exists()) {
		if (qdir.mkpath(dirpath)) {
			LOG_INFO("%s not exists and mkdir",qstrTOstr(dirpath).c_str())
		}
		else {
			LOG_ERR("%s not exists and mkdir fail",qstrTOstr(dirpath).c_str())
		}
	}

	int size = 0;
	QString dpath;
	if (robj == E_TABLEWIDGET) {
		size = ui.tablewidget->rowCount();
		dpath = dirpath + "/dat";

		for (int row = 0; row < size; row++) {
			QTableWidgetItem* it = ui.tablewidget->item(row, ECOL_FILENAME);
			if (!it) {
				return;
			}
			QVariant var = it->data(1);
			Data* data = (Data*)var.value<void*>();
			if (!data) {
				return;
			}
			list.groups << *data;
		}
	}
	else if (robj == E_COMPLETETABLEWIDGET) {
		size= ui.completetablewidget->rowCount();
		dpath = dirpath + "/hdat";

		for (int row = 0; row < size; row++) {
			Data data;
			data._name = ui.completetablewidget->item(row, ECOL_FILENAME)->text();
			data._time = ui.completetablewidget->item(row, ECOL_TIME)->text();
			data._filesize = ui.completetablewidget->item(row, ECOL_OTHERSIZE)->text();
			data._filepath = ui.completetablewidget->item(row, ECOL_PATH)->text();
			list.groups << data;
		}
	}

	QFile* qfile = new QFile(dpath);
	if (qfile->open(QIODevice::WriteOnly)) {
		LOG_INFO("record file open success");
		QDataStream stream(qfile);
		stream << list;
		qfile->close();
	}
	else {
		LOG_ERR("record file open fail");
	}
	delete qfile;
}

// 展示记录
void filetransfqt::showRecordList(QString& user, RecordedObject robj){
	QDir checkdir = "./user/" + user;
	if (!checkdir.exists()) {
		LOG_INFO("new user,no data to recover");		
		setting s;
		s.settingRestore();	   // 新的登录用户，对设置的内容进行还原
		return;
	}

	QString dpath;
	if (robj == E_TABLEWIDGET) {
		dpath = "./user/" + user + "/dat";
	}
	else if (robj == E_COMPLETETABLEWIDGET) {
		dpath = "./user/" + user + "/hdat";
	}


	QFile* qfile = new QFile(dpath);
	if (qfile->open(QIODevice::ReadWrite)) {
		LOG_INFO("show record file open success");
		QDataStream stream(qfile);
		DataList list;
		stream >> list;
		qfile->close();
		if (list.groups.size() != 0) {
			int size = 0;
			for (Data d : list.groups) {
				if (robj == E_TABLEWIDGET) {
					if (recoverInsertRowInfo(d, size++)) {
						ui.uploadbutton->setEnabled(true);
						ui.cleartablebutton->setEnabled(true);
						ui.deletebutton->setEnabled(true);
					}
				}
				else if (robj == E_COMPLETETABLEWIDGET) {
					insertInfoIntoCompleteTbwidget(d);
				}

			qDebug()<<"*d*"<< d._name << d._filepath << d._folder << d._filesize 
					<< d._progress  << d._dir_id << d._dir<< d._time;
			}
		}
	}
	else {
		LOG_ERR("show record file open fail");
	}
	delete qfile;
}

// 统计显示总速率
void filetransfqt::statisticsTotalRate(){
	double totalrate = 0;
	for (int row = 0; row < ui.tablewidget->rowCount(); row++) {
		QTableWidgetItem* it = ui.tablewidget->item(row, ECOL_FILENAME);
		if (!it) {
			return;
		}
		QVariant var = it->data(1);
		Data* data = (Data*)var.value<void*>();
		if (!data) {
			return;
		}
		if (data->_status == TRANSFERING) {
			totalrate += data->_speed;
		}
	}

	QString unit;
	double realtimeSpeed = totalrate;
	if (realtimeSpeed > UNIT_SIZE * UNIT_SIZE * UNIT_SIZE) {
		unit = " GB/s";
		realtimeSpeed /= UNIT_SIZE * UNIT_SIZE * UNIT_SIZE;
	}
	else if (realtimeSpeed > UNIT_SIZE * UNIT_SIZE) {
		unit = " MB/s";
		realtimeSpeed /= UNIT_SIZE * UNIT_SIZE;
	}
	else if (realtimeSpeed > UNIT_SIZE) {
		unit = " KB/s";
		realtimeSpeed /= UNIT_SIZE;
	}
	else if (realtimeSpeed < UNIT_SIZE) {
		unit = " B/s";
		realtimeSpeed /= 1;
	}

	QString totalSpeed = tr("%1").arg(realtimeSpeed, 0, 'f', 2) + unit;
	numlb->setText(totalSpeed);
}

// 恢复记录后需要换目录
void filetransfqt::changeDir(QString& dirpath, QString& dirId){
	for (int row = 0; row < ui.tablewidget->rowCount(); row++) {
		QTableWidgetItem* it = ui.tablewidget->item(row, ECOL_FILENAME);
		if (!it) {
			return;
		}
		QVariant var = it->data(1);
		Data* data = (Data*)var.value<void*>();
		if (!data) {
			return;
		}

		QString dir = data->_dir;    // 记录一下

		data->_dir = dirpath;
		data->_dir_id = dirId;
		data->_info = fp->initFileInfo(qstrTOstr(data->_filepath), qstrTOstr(data->_folder), qstrTOstr(dirpath));
		QString text = "上传目录已由 \"" + dir + "\" 更改为 \"" + dirpath + "\"";
		showHintLabel(this, text, 2500, 150, 60, "16px");

#ifdef _DEBUG
		qDebug() <<"changeDir" << data->_dir <<" " << data->_dir_id;
		std::cout << "changeDir" << data->_info->remoteFilePath << " " << data->_info->localRelativePath << endl;
#endif
	}
}

// 初始化列表
void filetransfqt::initCompleteTbwidget(){
	ui.completetablewidget->setColumnCount(ONEROWCOUNT);							 // 设置列										

	QStringList headers;                                                             // 设置标头
	headers << "文件名称" << "完成时间" << "文件大小" << "文件路径";
	ui.completetablewidget->setHorizontalHeaderLabels(headers);
	ui.completetablewidget->setStyleSheet("selection-background-color:rgb(193,210,240)");        // 设置选中单元格颜色
	ui.completetablewidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);  // 自适应宽度  4个可选项
	ui.completetablewidget->horizontalHeader()->setMinimumHeight(40);                            // 表头高度
	ui.completetablewidget->horizontalHeader()->setFont(QFont("Microsoft YaHei", 11));		     // 表头字体
	ui.completetablewidget->setColumnWidth(ECOL_FILENAME, 250);								     // 设置第一列的宽度
	ui.completetablewidget->setColumnWidth(ECOL_TIME, 200);
	ui.completetablewidget->setColumnWidth(ECOL_OTHERSIZE, 150);
	ui.completetablewidget->setColumnWidth(ECOL_PATH, 250);
	ui.completetablewidget->setEditTriggers(QAbstractItemView::NoEditTriggers);					 // 设置单元格不可编辑
	ui.completetablewidget->verticalHeader()->hide();                                            // verticalHeader  竖直表头  horizontalHeader  水平表头 -- 隐藏行号
	ui.completetablewidget->setShowGrid(false);													 // 隐藏网格线
	ui.completetablewidget->setSelectionBehavior(QAbstractItemView::SelectRows);				 // 设置选择行为，以行为单位
	ui.completetablewidget->setSelectionMode(QAbstractItemView::SingleSelection);				 // 设置选择模式，选择单行
	ui.completetablewidget->setSelectionMode(QAbstractItemView::ExtendedSelection);				 // 设置一次选择多行
	ui.completetablewidget->setFocusPolicy(Qt::NoFocus);										 // 隐藏选中框
	ui.completetablewidget->horizontalHeader()->setHighlightSections(false);					 // 取消表头的在选中单元格时的高亮状态

	ui.totalamountlineedit->setEnabled(false);

	connect(ui.clearallrecordbutton, SIGNAL(clicked()), this, SLOT(slots_clearAllrecord()));
}

// 插入信息到完成列表
void filetransfqt::insertInfoIntoCompleteTbwidget(Data& data,bool flag){
	int row = ui.completetablewidget->rowCount();
	ui.completetablewidget->insertRow(row);

	// 文件名称
	ui.completetablewidget->setItem(row, ECOL_FILENAME, new QTableWidgetItem(data._name));
	ui.completetablewidget->item(row, ECOL_FILENAME)->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);

	// 时间
	ui.completetablewidget->setItem(row, ECOL_TIME, new QTableWidgetItem(data._time));
	ui.completetablewidget->item(row, ECOL_TIME)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

	// 大小
	ui.completetablewidget->setItem(row, ECOL_OTHERSIZE , new QTableWidgetItem(data._filesize));
	ui.completetablewidget->item(row, ECOL_OTHERSIZE)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

	// 路径
	ui.completetablewidget->setItem(row, ECOL_PATH, new QTableWidgetItem(data._filepath));
	ui.completetablewidget->item(row, ECOL_PATH)->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);

	// 此处为了将每一次新插入的信息显示在第一行，做一个排序，不做移动。。
	ui.completetablewidget->sortByColumn(ECOL_TIME, Qt::DescendingOrder);/*AscendingOrder*/
}

// 登录退出通知
bool filetransfqt::logOut(QString& url, QString& qtoken){
	FtpProtocol http;
	string s = http.getLogout(qstrTOstr(url), qstrTOstr(qtoken));
	return getJsonOfLogout(strTOqstr_uft8(s));    // 中文字符strTOqstr_uft8（）
}

// 解析退出返回的Json信息
bool filetransfqt::getJsonOfLogout(QString& json){
	int code = -1;
	QString msg;
	QJsonParseError json_error;

	QJsonDocument json_document = QJsonDocument::fromJson(json.toStdString().data(), &json_error);
	if (!json_document.isNull() && (json_error.error == QJsonParseError::NoError)) {
		// QString 转 QJson 成功
		LOG_INFO("fromJson successed");
		if (json_document.isObject()) {
			QJsonObject json_object = json_document.object();
			if (json_object.contains("code")) {
				QJsonValue json_value = json_object.value("code");
				if (json_value.isDouble()) {
					code = json_value.toInt();
				}
			}

			if (code == 400) {                    // 失败
				if (json_object.contains("msg")) {
					QJsonValue json_value = json_object.value("msg");
					if (json_value.isString()) {
						msg = json_value.toString();
					}
				}

				if (json_object.contains("data")) {
					// TODO
				}
			}

			if (code == 0) {                      // 成功
				if (json_object.contains("msg")) {
					QJsonValue json_value = json_object.value("msg");
					if (json_value.isString()) {
						msg = json_value.toString();
					}
				}

				if (json_object.contains("data")) {
					// TODO
				}

			}
		}
	}

#ifdef _DEBUG
	qDebug() << "code" << code << "verificationResult" << msg ;
#endif

	QString verificationSuccess = "退出成功";
	QString verificationFail = "token验证失败";
	QString unexpected ="Exit unexpected error";
	if (code == 0 && msg == verificationSuccess) {
		LOG_INFO("code: %d %s",code, qstrTOstr(verificationSuccess).c_str());
		return true;
	}
	else if (code == 400 && msg == verificationFail){
		LOG_ERR("code: %d %s",code, qstrTOstr(verificationFail).c_str());
		return false;
	}
	else {
		LOG_ERR("code %d %s",code ,qstrTOstr(unexpected).c_str() );
		return false;
	}
}

// 解析上传完成回调Json信息
bool filetransfqt::getJsonOfUpBack(QString& json){
	int code = -1;
	QString msg;
	QJsonParseError json_error;

	QJsonDocument json_document = QJsonDocument::fromJson(json.toStdString().data(), &json_error);
	if (!json_document.isNull() && (json_error.error == QJsonParseError::NoError)) {
		// QString 转 QJson 成功
		LOG_INFO("fromJson successed");
		if (json_document.isObject()) {
			QJsonObject json_object = json_document.object();
			if (json_object.contains("code")) {
				QJsonValue json_value = json_object.value("code");
				if (json_value.isDouble()) {
					code = json_value.toInt();
				}
			}

			if (code == 200) {
				if (json_object.contains("msg")) {
					QJsonValue json_value = json_object.value("msg");
					if (json_value.isString()) {
						msg = json_value.toString();
					}
				}

				if (json_object.contains("data")) {
					// TODO
				}

			}
		}
	}
	else {
		LOG_ERR("fromJson failed");
		return false;
	}
	LOG_INFO("%d,%s", code, qstrTOstr(msg).c_str());
	return true;
}

// 文件过滤
bool filetransfqt::fileTypeFilter(QString& filepath){
	QString s = filepath.right(filepath.size() - filepath.lastIndexOf(".") - 1);

// 	int pos = filepath.lastIndexOf(".") + 1;
// 	QString s = filepath.mid(pos);             // 与前一句同义

	int i = 0;
	for (; i < fileType.size(); i++) {
		if (s == fileType[i]) {
			return true;
		}
	}

	QString text = "当前用户不被允许上传 ." + s + " 类型文件";
	showHintLabel(this, text, 1500, 150, 60, "16px");
	return false;
}

// 计算大小结束，启动上传
void filetransfqt::slots_receiveSuccessFlag(Data *data){
	if (!data) {
		return;
	}
	QString filepath = data->_filepath;
	int row = getRowOfFilename(qstrTOstr(filepath), status[WAITING]);
	if (row == -1) {
		return;
	}

	// When the calculation process is not over,
	// the user pauses the current task, 
	// but the thread task completes the next task. 
	// When it comes to this, you need to check TRANSFERINGCOUNT--
	if (data->_status == STOP && data->_status_prev == WAITING) { 
		_mutex.lock();
		TRANSFERINGCOUNT--;
		_mutex.unlock();
        qDebug() << "stop-wait TRANSFERINGCOUNT--" << TRANSFERINGCOUNT;
		return;
	}

	data->_status_prev = data->_status;
	data->_status = TRANSFERING;
	ui.tablewidget->item(row, ECOL_STATUS)->setText(statusName[data->_status]);
	
	if (!thread->isRunning()) {
		thread->start();
		LOG_INFO("upload thread start");
	}
}

// 失败重试
void filetransfqt::slots_receiveRetry() {
	int rowCount = ui.tablewidget->rowCount();
	if (!rowCount) {
		return;
	}
	
	Data* data;
	for (int row = 0; row < rowCount; ++row) {
		QTableWidgetItem* it = ui.tablewidget->item(row, ECOL_FILENAME);
		if (!it) {
			return;
		}
		QVariant var = it->data(1);
		Data* data = (Data*)var.value<void*>();
		if (!data) {
			return;
		}
		if (data->_status == FINSHED || 
			data->_status == STOP || 
			data->_status == WAITING || 
			data->_status == TRANSFERING) {

			continue;
		}
		
		if (data->_status == FAIL ) {
			emit this->signal_sendData(data);
		}

		LOG_INFO("%s upload retry!!!", qstrTOstr(data->_filepath).c_str());
	}
}

// 速率值
void filetransfqt::slots_receiveSpeed(double speed,string filepath){
	int row = getRowOfFilename(filepath, status[TRANSFERING] | status[FINSHED]);
	if (row == -1) {
		return;
	}

	QTableWidgetItem* it = ui.tablewidget->item(row, ECOL_FILENAME);
	if (!it) {
		return;
	}
	QVariant var = it->data(1);
	Data* data = (Data*)var.value<void*>();
	if (!data) {
		return;
	}

	data->_speed = speed;

	QString unit;
	double realtimeSpeed = speed;
	if (realtimeSpeed > UNIT_SIZE * UNIT_SIZE * UNIT_SIZE)	{
		unit = " GB/s";
		realtimeSpeed /= UNIT_SIZE * UNIT_SIZE * UNIT_SIZE;
	}
	else if (realtimeSpeed > UNIT_SIZE * UNIT_SIZE)	{
		unit = " MB/s";
		realtimeSpeed /= UNIT_SIZE * UNIT_SIZE;
	}
	else if (realtimeSpeed > UNIT_SIZE)	{
		unit = " KB/s";
		realtimeSpeed /= UNIT_SIZE;
	}
	else if (realtimeSpeed < UNIT_SIZE)	{
		unit = " B/s";
		realtimeSpeed /= 1;
	}

	QString uploadSpeed = tr("%1").arg(realtimeSpeed, 0, 'f', 2) + unit;
 	ui.tablewidget->item(row, ECOL_SPEED)->setText(uploadSpeed);
}

// 当前完成值
void filetransfqt::slots_receiveSize(double rmt, double loc, string filepath){
	int row = getRowOfFilename(filepath, status[TRANSFERING] | status[FINSHED]);
	if (row == -1) {
		return;
	}

	double locsize = loc;
	double rmtsize = rmt;
	QString unit(" B");
	if (loc > UNIT_SIZE) { 
		locsize = KB(loc);
		rmtsize = KB(rmt);
		unit = " KB";
		if (loc > UNIT_SIZE * UNIT_SIZE) {
			locsize = MB(loc);
			rmtsize = MB(rmt);
			unit = " MB";

			if (loc > UNIT_SIZE * UNIT_SIZE * UNIT_SIZE) {
				locsize = GB(loc);
				rmtsize = GB(rmt);
				unit = " GB";
			}
		}
	}

	QString sRemote = tr("%1").arg(rmtsize, 0, 'f', 2) + unit;
	QString sLocal=tr("%2").arg(locsize, 0, 'f', 2) + unit;
	QString s = sRemote +'/'+ sLocal;
 	ui.tablewidget->item(row, ECOL_COMPLETED)->setText(s);
}

// 预计完成时间
void filetransfqt::slots_receiveTime(string estimateTime, string filepath){
	int row = getRowOfFilename(filepath, status[TRANSFERING] | status[FINSHED]);
	if (row == -1) {
		return;
	}

	ui.tablewidget->item(row, ECOL_TIME)->setText(strTOqstr(estimateTime));
}

// 进度值
void filetransfqt::slots_receiveValue(double value, string filepath) {
	int row = getRowOfFilename(filepath, status[TRANSFERING]| status[FINSHED]);
	if (row == -1) {
		return;
	}

	QProgressBar* progress = (QProgressBar*)ui.tablewidget->cellWidget(row, ECOL_PRO);
	if (progress) {
		progress->setValue(value);
	}

	if (value == 100) {
		QTableWidgetItem* it = ui.tablewidget->item(row, ECOL_FILENAME);
		if (!it) {
			return;
		}
		QVariant var = it->data(1);
		Data* data = (Data*)var.value<void*>();
		if (!data) {
			return;
		}

		if (TRANSFERINGCOUNT > 0) {
			_mutex.lock();
			TRANSFERINGCOUNT--;
			_mutex.unlock();
		}
		qDebug() << "complete TRANSFERINGCOUNT--" << TRANSFERINGCOUNT;
		if (TRANSFERINGCOUNT == 0) {
			ui.uploadbutton->setEnabled(true);
		}
		
		QString dateTime = QDateTime::currentDateTime().toString("yyyy/MM/dd hh:mm");
		QString dateTime_one = QDateTime::currentDateTime().toString("yyyy/MM/dd hh:mm:ss");
		ui.tablewidget->item(row, ECOL_TIME)->setText(dateTime);
		ui.tablewidget->item(row, ECOL_SPEED)->setText("");

		data->_time = dateTime_one;      //记录时间	
	}
}

// 底层回调， 参数列表：filetransfqt对象、文件全路径、预计完成时间、速率、进度、当前完成大小、总大小
int filetransfqt::uploadInfoCb(void* obj, string filepath, string estimateTime, double speed,
									  double progress, long long nowSize, long long localSize){
	
	if (!obj) {
		return 0;
	}
	filetransfqt* ft = (filetransfqt*)obj;

	if(ft){
		ft->outgivingInfo(obj, filepath, estimateTime, speed, progress, nowSize, localSize);
	}
	return 0;
}

// 分发消息
void filetransfqt::outgivingInfo(void* obj, string filepath, string estimateTime,
							double speed, double progress, long long nowSize, long long localSize) {
	
	if (!obj) {
		return;
	}

	filetransfqt* ft = (filetransfqt*)obj;	
	if (!ft ) {
		return;
	}

	int row = ft->getRowOfFilename(filepath, status[TRANSFERING]);
	if (row == -1) {
		return;
	}
	
	QTableWidgetItem* it = ui.tablewidget->item(row, ECOL_FILENAME);
	if (!it) {
		return;
	}
	QVariant var = it->data(1);
	Data* data = (Data*)var.value<void*>();
	if (!data) {
		return;
	}

	if (ft) {
		if (data->_status == TRANSFERING) {
			emit ft->signal_sendSpeed(speed, filepath);	                  // 速率  	
			string s = speed > 0 ? estimateTime : "--:--:--";
			ft->statisticsTotalRate();                                    // 统计速率
			emit ft->signal_sendTime(s, filepath);						  // 预计完成时间
			emit ft->signal_sendSize(nowSize, localSize, filepath);       // 当前完成大小

			if (progress - data->_progress >= 1) {
				emit ft->signal_sendValue(progress, filepath);	          // 进度
				data->_progress = progress;
			}
			if (nowSize == localSize) {
				data->_status_prev = data->_status;
				data->_status = FINSHED;
				data->_speed = 0.0;                                       // 保险起见，置0，避免误差
				ui.tablewidget->item(row, ECOL_STATUS)->setText(statusName[data->_status]);
				if (!data->_bstatu) {
					emit this->signal_sendAdd(1);
				}
				emit this->signal_sendCompleted(data);					 // 完成通知	
				ft->statisticsTotalRate();                               // 完成清0
			}
		}
	}
}

// 接收失败的任务
int filetransfqt::rece(void* obj, string& url){
	if (!obj) {
		return 0;
	}
	filetransfqt* ft = (filetransfqt*)obj;
	if (ft) {
		ft->dealFailedTasks(obj, url);
	}
	return 0;
}

// 处理失败的任务
void filetransfqt::dealFailedTasks(void* obj, string& url){
	if (!obj) {
		return;
	}
	filetransfqt* ft = (filetransfqt*)obj;
	int row = getRowOfFilename(url, status[TRANSFERING], false);
	QTableWidgetItem* it = ui.tablewidget->item(row, ECOL_FILENAME);
	if (!it) {
		return;
	}
	QVariant var = it->data(1);
	Data* data = (Data*)var.value<void*>();
	if (!data) {
		LOG_ERR("void* to Data* failed")
		return;
	}
	data->_status_prev = data->_status;
	data->_status = FAIL;
	fp->dealFailtaskHandle(data->_info); // 处理失败的句柄


	if (TRANSFERINGCOUNT > 0) {
		_mutex.lock();
		TRANSFERINGCOUNT--;
		_mutex.unlock();
	}

	qDebug() << "fail TRANSFERINGCOUNT--" << TRANSFERINGCOUNT;

	ui.tablewidget->item(row, ECOL_STATUS)->setText(statusName[data->_status]);
	ui.tablewidget->item(row, ECOL_SPEED)->setText(" ");
	ui.tablewidget->item(row, ECOL_TIME)->setText(" ");
}

// 用户选项
void filetransfqt::slots_receivePromptInfo(Data* data){
	if (!data) {
		return;
	}
	int da = configIni->value("SETTINGS/defaultActionId").toInt();
	bool op = configIni->value("SETTINGS/defaultOption").toBool();
	qDebug() << "da" << da << "op" << op;

	if (da == 0 && !op) {
		options ow ;
		ow.showDuplicateInfo(data, path);
		ow.exec();
		RbStatu* rbs = ow.getOptions();

#ifdef _DEBUG
		qDebug() << rbs->id << rbs->statu << rbs->fileName;
#endif

		if (rbs->id == E_RESUME && rbs->statu) {
			LOG_INFO("%s ,user choose resume!!!", qstrTOstr(data->_filepath).c_str());
			fp->initEasyhandleOfUl(data->_info);
			fp->addEasyhandle(data->_info);
			slots_receiveSuccessFlag(data);
		}
		else if (rbs->id == E_COVER && rbs->statu) {
			LOG_INFO("%s ,user choose cover!!!", qstrTOstr(data->_filepath).c_str());
			fp->coverRemoteFile(data->_info);
			fp->initEasyhandleOfUl(data->_info);
			fp->addEasyhandle(data->_info);
			slots_receiveSuccessFlag(data);
		}
		else if (rbs->id == E_RENAME && rbs->statu) {
			LOG_INFO("%s ,user choose rename!!!", qstrTOstr(data->_filepath).c_str());
			QString s = rbs->fileName;
			fp->renameRemotePath(data->_info, qstrTOstr(s));
			
			bool ret = fp->calcuRemoteSize(data->_info);
			qDebug() << "rename,calcu size" << ret << data->_info->remoteFileSize ;
			if (ret && data->_info->remoteFileSize != 0) {
				LOG_INFO("%s,user choose rename,but the same name still exists",data->_info->localRelativePath.c_str());
				slots_receivePromptInfo(data);
			}
			else if (ret && data->_info->remoteFileSize == 0) {
				LOG_INFO("%s,user choose rename,the same name does not exist", data->_info->localRelativePath.c_str());
				fp->initEasyhandleOfUl(data->_info);
				fp->addEasyhandle(data->_info);
				slots_receiveSuccessFlag(data);
			}
			else if (!ret) {
				LOG_INFO("%s,user choose rename,but calcu size fail,retry", data->_info->localRelativePath.c_str());
				slots_receivePromptInfo(data);
			}
		}
		else if (rbs->id == E_CANCEL && rbs->statu) {
			LOG_INFO("%s ,user choose cancel!!!", qstrTOstr(data->_filepath).c_str());
			fp->deleteInfo(data->_info);

			if (TRANSFERINGCOUNT > 0) {
				_mutex.lock();
				TRANSFERINGCOUNT--;
				_mutex.unlock();
			}
			qDebug() << "4 TRANSFERINGCOUNT--" << TRANSFERINGCOUNT;
			if (TRANSFERINGCOUNT == 0) {
				ui.uploadbutton->setEnabled(true);
				if (!data->_bstatu) {
					emit this->signal_sendAdd(1);
				}
			}
			deleteData(getRowOfFilename(qstrTOstr(data->_filepath), status[WAITING]));
		}
		else {
			// 未设定的值
		}

	}
	else if (da == 1 && op) {
		LOG_INFO("default option is cover!!!");
		fp->coverRemoteFile(data->_info);
		fp->initEasyhandleOfUl(data->_info);
		fp->addEasyhandle(data->_info);
		slots_receiveSuccessFlag(data);
	}
	else if (da == 2 && op){
		LOG_INFO("default option is resume!!!");
		fp->initEasyhandleOfUl(data->_info);
		fp->addEasyhandle(data->_info);
		slots_receiveSuccessFlag(data);
	}
	else if (da == 3 && op) {
		LOG_INFO("default option is rename!!!");
		newname nn;
		nn.setOldname(getName(data->_name));
		if (nn.exec() == QDialog::Accepted) {
			QString sname = nn.getNewname();
			fp->renameRemotePath(data->_info, qstrTOstr(sname));
			bool bret = fp->calcuRemoteSize(data->_info);

			qDebug() << "bret" << bret << "remoteFileSize" << data->_info->remoteFileSize;

			if (bret && data->_info->remoteFileSize != 0) {
				LOG_INFO("%s,default option is rename,but the same name still exists", data->_info->localRelativePath.c_str());
				slots_receivePromptInfo(data);
			}
			else if (bret && data->_info->remoteFileSize == 0) {
				LOG_INFO("%s,default option is rename,the same name does not exist", data->_info->localRelativePath.c_str());
				fp->initEasyhandleOfUl(data->_info);
				fp->addEasyhandle(data->_info);
				slots_receiveSuccessFlag(data);
			}
			else if (!bret) {
				LOG_INFO("%s,default option is rename,but calcu size fail,retry", data->_info->localRelativePath.c_str());
				slots_receivePromptInfo(data);
			}
		}
		else {
			// 同取消的操作
			LOG_INFO("%s ,default option is cancel!!!", qstrTOstr(data->_filepath).c_str());

			if (TRANSFERINGCOUNT > 0) {
				_mutex.lock();
				TRANSFERINGCOUNT--;
				_mutex.unlock();
			}
			qDebug() << "4 TRANSFERINGCOUNT--" << TRANSFERINGCOUNT;
			if (TRANSFERINGCOUNT == 0) {
				ui.uploadbutton->setEnabled(true);
				if (!data->_bstatu) {
					qDebug() << "*****";
					emit this->signal_sendAdd(1);
				}
			}
			fp->deleteInfo(data->_info);
			deleteData(getRowOfFilename(qstrTOstr(data->_filepath), status[WAITING]));
		}
	}
}

// 系统托盘点击事件
void filetransfqt::slots_iconActivated(QSystemTrayIcon::ActivationReason ireason){
	switch (ireason)	{
	case QSystemTrayIcon::Trigger: 
		this->showNormal();
		break;
	case QSystemTrayIcon::DoubleClick: 
		this->showNormal();
		break;
	case QSystemTrayIcon::MiddleClick:
		break;
	default:
		break;
	}
}

// 显示主界面
void filetransfqt::slots_showMainpage(){
	ui.transfercompletebutton->setStyleSheet("QPushButton{ font: 15px \"Microsoft YaHei\"; color:rgb(0, 0, 0) }");
	ui.mainbutton->setStyleSheet("QPushButton{ font: 15px \"Microsoft YaHei\"; color:rgb(6, 168, 255) }");

//	ui.stackwidget->setCurrentIndex(0);                  // 索引显示
	ui.stackwidget->setCurrentWidget(ui.mainpage);       // 窗口名显示
}
 
// 显示传输完成
void filetransfqt::slots_showTransferCompletepage(){
//	ui.stackwidget->setCurrentIndex(1);                           // 索引显示
	ui.stackwidget->setCurrentWidget(ui.transfercompletepage);    // 窗口名显示
	
	ui.transfercompletebutton->setStyleSheet("QPushButton{ font: 15px \"Microsoft YaHei\"; color:rgb(6, 168, 255) }");
	ui.mainbutton->setStyleSheet("QPushButton{ font: 15px \"Microsoft YaHei\"; color:rgb(0, 0, 0) }");
	
	int count = ui.completetablewidget->rowCount();
	QString num = QString::number(count);
	QString text = "共传输完成" + num + "个文件";
	ui.totalamountlineedit->setText(text);

	if (count != 0) {
		ui.clearallrecordbutton->setEnabled(true);
	}
	else {
		ui.clearallrecordbutton->setEnabled(false);
	}
}

// 递归添加子目录
void filetransfqt::addChildNode(QTreeWidgetItem* parent, QString& name, QString& nid, QString& npid){
	for (int i = 0; i < parent->childCount(); i++) {
		QTreeWidgetItem* child = parent->child(i);
		if (child->childCount() != 0) {
			addChildNode(child, name, nid, npid);
		}
		QString sid = child->text(1);
		if (sid == npid) {
			addTreeNode(child, name, nid);
			break;
		}
	}
}

// 处理目录树选中的子目录
void filetransfqt::slots_onedir_check(QTreeWidgetItem* item, int cloumn) {  
	if (!item) {
		return;
	}
	QStringList nodepath;                    // 存储路径
	QTreeWidgetItem* itemfile = item;        // 被点击的item   
	QStringList itemid;                      // 获取到的当前item 的id

	while (itemfile != NULL){
		itemid << itemfile->text(1);         // itemfile id
		nodepath << itemfile->text(0);       // itemfile名称
		itemfile = itemfile->parent();       // 将itemfile指向父item
	}

	QString dirpath;
	int size = nodepath.size();
	for (int i = size - 1; i >= 0; i--) {    // QStringlist类filepath反向存着初始item的路径
		dirpath += nodepath.at(i);			 // nodepath反向输出，相应的加入’/‘
		if (i != 0) {
			dirpath += "/";
		}
		
	}
	dirpath += "/";                          // 所有需要拼接的路径均已 ‘/’ 结尾

#ifdef _DEBUG
	qDebug() << dirpath << itemid.at(0) << "id" << cloumn;
#endif
	
// 	// 更改上传位置   暂定
// 	if (childpath != dirpath) {
// 		QString dirId = itemid.at(0);
// 		changeDir(dirpath, dirId);
// 	}

	childpath = dirpath;              
	childpathId = itemid.at(0);

	// 提醒
	showHintLabel(ui.dirtreewidget, dirpath, 1000, 50, 30, "14px");
	ui.statusBar->showMessage("当前目录: "+dirpath);
	
	configIni->setValue("LastPath/dirpath", QVariant(childpath));
	configIni->setValue("LastPath/dirpathId", QVariant(childpathId));
}

// 焦点显示最近一次的路径
QString filetransfqt::defaultDir(){
	QString dirpath = configIni->value("LastPath/dirpath").toString();
	QString dirpathId = configIni->value("LastPath/dirpathId").toString();

	QTreeWidgetItemIterator it(ui.dirtreewidget);
	
	while (*it) {
		if (dirpathId == (*it)->text(1)) {
			break;
		}
		it++;
	}
	ui.dirtreewidget->setFocus();
	ui.dirtreewidget->setCurrentItem(*it);

	return dirpath;
}

// 系统托盘退出
void filetransfqt::slots_trayIconClose(){
	this->hide();
	trayIcon->hide();

	fp->removehandle();
	disconnect(this, SIGNAL(signal_sendData(Data*)), fsthread, SLOT(slots_receiveData(Data*)));
	disconnect(fsthread, SIGNAL(signal_sendPromptInfo(Data*)), this, SLOT(slots_receivePromptInfo(Data*)));
	disconnect(fsthread, SIGNAL(signal_sendSuccessFlag(Data*)), this, SLOT(slots_receiveSuccessFlag(Data*)));
	disconnect(thread, SIGNAL(signal_sendRetry()), this, SLOT(slots_receiveRetry()));
	disconnect(this, SIGNAL(signal_sendAdd(int)), this, SLOT(slots_receiveAdd(int)));

	if (fsthread) {
		fsthread->ExitTh();
		while (fsthread->isRunning()) {
			fsthread->wait(1);
		}

		delete fsthread;
		fsthread = nullptr;
	}

	Sleep(100);
	clearlistitem();


	// 关闭所有线程						 
	if (fthread) {
		fthread->ExitTh();
		while (fthread->isRunning()) {
			fthread->wait(1);
		}
		delete fthread;
		fthread = nullptr;
	}
	if (thread) {
		while (thread->isRunning()) {
			thread->ExitTh();
			thread->wait(1);
		}
		delete thread;
		thread = nullptr;
	}

	Sleep(500);
	deleteInfo();

	recordList(username, E_TABLEWIDGET);
	recordList(username, E_COMPLETETABLEWIDGET);
	exitClear();

	LOG_INFO("user close app");

	QString hostkey = "logout/host";
	QString host = configIni->value(hostkey).toString();
	QString urlkey = "host/host";
	QString url = configIni->value(urlkey).toString();
	QString path = url + host;
	logOut(path, token);

	configIni->remove("allowtype");   // 这个数组清空
}

// 列排序
void filetransfqt::slots_sortByColumn(int colm) {
	if (colm == ECOL_FILENAME || colm == ECOL_TIME || colm == ECOL_STATUS ) {
		ui.tablewidget->sortByColumn(colm, ui.tablewidget->horizontalHeader()->sortIndicatorOrder());
		ui.tablewidget->horizontalHeader()->setSortIndicator(colm, ui.tablewidget->horizontalHeader()->sortIndicatorOrder());
		ui.tablewidget->horizontalHeader()->setSortIndicatorShown(true);	 // 显示升降序的箭头	
	}
	else if(colm == ECOL_SIZE){
		ui.tablewidget->sortByColumn(colm, ui.tablewidget->horizontalHeader()->sortIndicatorOrder());
		ui.tablewidget->horizontalHeader()->setSortIndicatorShown(true);
	}
	else {
		// 其他列不响应排序
		ui.tablewidget->horizontalHeader()->setSortIndicatorShown(false);
	}
}

// 清空所有记录
void filetransfqt::slots_clearAllrecord(){
	for (int row = ui.completetablewidget->rowCount() - 1; row >= 0;row--) {
		delete ui.completetablewidget->item(row, ECOL_FILENAME);
		delete ui.completetablewidget->item(row, ECOL_TIME);
		delete ui.completetablewidget->item(row, ECOL_OTHERSIZE);
		ui.completetablewidget->removeRow(row);
	}

	int count = ui.completetablewidget->rowCount();
	QString num = QString::number(count);
	QString text = "共传输完成" + num + "个文件";
	ui.totalamountlineedit->setText(text);
	ui.clearallrecordbutton->setEnabled(false);
}

// 退出清理完成列表
void filetransfqt::exitClear() {
	for (int row = ui.completetablewidget->rowCount() - 1; row >= 0; row--) {
		delete ui.completetablewidget->item(row, ECOL_FILENAME);
		delete ui.completetablewidget->item(row, ECOL_TIME);
		delete ui.completetablewidget->item(row, ECOL_OTHERSIZE);
		ui.completetablewidget->removeRow(row);
	}
}

// 拖入事件
void filetransfqt::dragEnterEvent(QDragEnterEvent * event){	
	event->setDropAction(Qt::MoveAction);
	event->accept();
}

// 放下事件
void filetransfqt::dropEvent(QDropEvent * event) {
	int rowCount = ui.tablewidget->rowCount();
	QList <QUrl> urls = event->mimeData()->urls();
	if (urls.isEmpty()) {
		return;
	}

	bool bret = false;
	QList < QUrl > ::iterator it;
	for (it = urls.begin(); it != urls.end(); it++) {
		QFileInfo fileInfo(it->toLocalFile());
		if (fileInfo.isFile()) {               //是文件就直接加进来
			bret = insertRowInfo(fileInfo, "", rowCount);
		}
		else if (fileInfo.isDir()){            //文件路径就循环读取
			QFileInfoList fileInfoList = getAllFileList(fileInfo.filePath());
			for (QFileInfo filedirInfo : fileInfoList) {
				bret = insertRowInfo(filedirInfo, fileInfo.filePath(), rowCount);
			}
		}
	}

	if (bret) {
		ui.cleartablebutton->setEnabled(true);
		ui.deletebutton->setEnabled(true);
		ui.uploadbutton->setEnabled(true);
	}
}

// 鼠标点击事件
void filetransfqt::mousePressEvent(QMouseEvent* event) {
	if (event->button()== Qt::LeftButton) {  // 鼠标左键
		int row = ui.tablewidget->currentRow();
		qDebug() << row << endl;
	}
	else {
		event->ignore();
	}
}

// 响应鼠标事件
bool filetransfqt::eventFilter(QObject* obj, QEvent* e){
	if (obj == ui.tablewidget->viewport())	{
		switch (e->type())
		{
		case QEvent::MouseButtonRelease:
		{
			emit signal_buttonRelease();
			break;
		}
		case QEvent::MouseButtonDblClick:
		{
			emit signal_buttonDblClick();
			break;
		}
		default:
			break;
		}
	}
	return QWidget::eventFilter(obj, e);
}

// 鼠标捕获，显示tip
void filetransfqt::slots_showToolTip(QModelIndex index){
	if (!index.isValid()) {
		LOG_ERR("Invalid index");
		return;
	}

	if (index.column() == 0) {                // 限制只显示第一列的，下标0
		QTableWidgetItem* it = ui.tablewidget->item(index.row(), ECOL_FILENAME);
		if (!it) {
			return;
		}
		QVariant var = it->data(1);
		Data* data = (Data*)var.value<void*>();
		if (!data) {
			return;
		}
		QString info = data->_filepath + "\n 文件大小: " + data->_filesize;
		QToolTip::showText(QCursor::pos(),info);
	}
}

// 自动删除
void filetransfqt::slots_receiveAutomaticDel(Data* data, QString& res){
	if (!data) {
		return;
	}
	int row = getRowOfFilename(qstrTOstr(data->_filepath), status[FINSHED]);
	if (row == -1) {
		return;
	}

	// 通知结果解析
	getJsonOfUpBack(res);
	// 插入完成列表
	insertInfoIntoCompleteTbwidget(*data, true);
	
	Sleep(1000);  // 等待回调的记录完成的时间

	// 自动移除
	deleteInfo(row);
	deleteData(row);

	if (ui.tablewidget->rowCount() == 0) {
		ui.uploadbutton->setEnabled(false);
		ui.startbutton->setEnabled(false);
		ui.stopbutton->setEnabled(false);
		ui.deletebutton->setEnabled(false);
		ui.cleartablebutton->setEnabled(false);
	}
}

// 窗体鼠标左键双击事件
void filetransfqt::slots_buttonDblClick(){
	QList<QTableWidgetItem*> selection = ui.tablewidget->selectedItems();
	if (selection.isEmpty())
		return;

	vector<int> itemIndex;								                          // 存储当前选中的行号
	for (int i = 0; i < selection.size(); ++i) {
		itemIndex.push_back(selection.at(i)->row());                              // 存储
	}
	sort(itemIndex.begin(), itemIndex.end(), CompareAscending);	                  // 排序
	itemIndex.erase(unique(itemIndex.begin(), itemIndex.end()), itemIndex.end()); // 去重

	for (int i = 0; i < itemIndex.size(); ++i) {
		QTableWidgetItem* it = ui.tablewidget->item(itemIndex[i], ECOL_FILENAME);
		if (!it) {
			return;
		}
		QVariant var = it->data(1);
		Data* data = (Data*)var.value<void*>();
		if (!data) {
			return;
		}

		switch (data->_status)
		{
		case FAIL:
		case NOTSTART:
		case STOP:
		{			
			slots_start_check();
			break;
		}
		case WAITING:
		case TRANSFERING:
		{
			slots_stop_check();
			break;
		}
		default:
			break;
		}
	}
}

// 鼠标拖拽多选
void filetransfqt::slots_buttonRelease(){
	ui.stopbutton->setEnabled(false);
	ui.startbutton->setEnabled(false);
	QList<QTableWidgetItem*> selection = ui.tablewidget->selectedItems();
	if (selection.isEmpty())
		return;

	vector<int> itemIndex;								                          // 存储当前选中的行号
	for (int i = 0; i < selection.size(); ++i) {
		itemIndex.push_back(selection.at(i)->row());                              // 存储
	}
	sort(itemIndex.begin(), itemIndex.end(), CompareAscending);	                  // 排序
	itemIndex.erase(unique(itemIndex.begin(), itemIndex.end()), itemIndex.end()); // 去重

	for (int i = 0; i < itemIndex.size(); ++i) {
		QTableWidgetItem* it = ui.tablewidget->item(itemIndex[i], ECOL_FILENAME);
		if (!it) {
			return;
		}
		QVariant var = it->data(1);
		Data* data = (Data*)var.value<void*>();
		if (!data) {
			return;
		}

		switch (data->_status)
		{
		case FAIL:
		case NOTSTART:
		case STOP:
		{
			//ui.stopbutton->setEnabled(true);
			ui.startbutton->setEnabled(true);
			break;
		}
		case WAITING:
		case TRANSFERING:
		{
			ui.stopbutton->setEnabled(true);
			break;
		}
		default:
			break;
		}
	}
}

// 关闭事件
void filetransfqt::closeEvent(QCloseEvent *event){
	int id = configIni->value("SETTINGS/rbId").toInt();             // 获得radiobt id
	bool statu = configIni->value("SETTINGS/rbStatu").toBool();     // 获得radiobt statu

	if (id == 0) {
		if (trayIcon && trayIcon->isVisible()) {
			hide();                                                 // 隐藏窗口
			event->ignore();                                        // 忽略关闭事件
		}
		LOG_INFO("user minimizeAction !!!");
	}
	else if (id == 1) {
		event->accept();
		this->hide();
		trayIcon->hide();

		fp->removehandle();
		disconnect(this, SIGNAL(signal_sendData(Data*)), fsthread, SLOT(slots_receiveData(Data*)));
		disconnect(fsthread, SIGNAL(signal_sendPromptInfo(Data*)), this, SLOT(slots_receivePromptInfo(Data*)));
		disconnect(fsthread, SIGNAL(signal_sendSuccessFlag(Data*)), this, SLOT(slots_receiveSuccessFlag(Data*)));
		disconnect(thread, SIGNAL(signal_sendRetry()), this, SLOT(slots_receiveRetry()));
		disconnect(this, SIGNAL(signal_sendAdd(int)), this, SLOT(slots_receiveAdd(int)));

		if (fsthread) {
			fsthread->ExitTh();
			while (fsthread->isRunning()) {
				fsthread->wait(1);
			}

			delete fsthread;
			fsthread = nullptr;
		}

		Sleep(500);
		clearlistitem();

		// 关闭所有线程						 
		if (fthread) {
			fthread->ExitTh();
			while (fthread->isRunning()) {
				fthread->wait(1);
			}
			delete fthread;
			fthread = nullptr;
		}
		if (thread) {
			while (thread->isRunning()) {
				thread->ExitTh();
				thread->wait(1);
			}
			delete thread;
			thread = nullptr;
		}

		recordList(username,E_TABLEWIDGET);
		recordList(username, E_COMPLETETABLEWIDGET);
		exitClear();

		Sleep(500);
		deleteInfo();

		LOG_INFO("user close app");

		QString hostkey = "logout/host";
		QString host = configIni->value(hostkey).toString();
		QString urlkey = "host/host";
		QString url = configIni->value(urlkey).toString();
		QString path = url + host;
		logOut(path, token);

		configIni->remove("allowtype");   // 这个数组清空
		Sleep(500);
	}
}

// 任务栏隐藏事件
void filetransfqt::hideEvent(QHideEvent* event){
	// 这个会导致最小化后，在右下角打开，不会变成顶层窗口
// 	if (trayIcon && trayIcon->isVisible())	{
// 		hide();          //隐藏窗口
// 		event->ignore(); //忽略事件
// 	}	
}

// 递归打开文件夹
QFileInfoList filetransfqt::getAllFileList(QString& path) {
	QDir dir(path);
	QFileInfoList filelist = dir.entryInfoList(QDir::Files | QDir::Hidden | QDir::NoSymLinks);
	QFileInfoList folderlist = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

	for (int i = 0; i != folderlist.size(); i++) {
		QString name = folderlist.at(i).absoluteFilePath();
		QFileInfoList child_file_list = getAllFileList(name);
		filelist.append(child_file_list);
	}
	return filelist;
}

// 信息插入行
bool filetransfqt::insertRowInfo(QFileInfo& fileInfo, QString folder, int row) {
	int rowcount = ui.tablewidget->rowCount();
	// 同一文件处理
	for (int i = 0; i < rowcount; ++i) {
		QTableWidgetItem* it = ui.tablewidget->item(i, ECOL_FILENAME);
		if (!it) {
			return false;
		}
		QVariant var = it->data(1);
		Data* data = (Data*)var.value<void*>();
		if (!data) {
			return false;
		}
		if (fileInfo.filePath() == data->_filepath) {
			QString text = fileInfo.filePath() + "\n 已存在列表!!";
			showHintLabel(this, text, 1500, 150, 70, "16px");
			return false;
		}
	}
	
	// 文件格式的控制 
	if (!fileTypeFilter(fileInfo.filePath())) {
		return false;
	}

	ui.tablewidget->insertRow(row);

	// 初始化data
	Data* data = new Data;
	data->_name = fileInfo.fileName();
	data->_filepath = fileInfo.filePath();
	data->_folder = folder;
	data->_status = NOTSTART;
	data->_bstatu = false;
	data->_status_prev = data->_status;
	data->_progress = 0;
	data->_host = configIni->value("host/host").toString() + configIni->value("upback/host").toString();;
	data->_token = token;
	data->_dir = childpath;
	data->_dir_id = childpathId;
	data->_speed = 0.0;
	data->_info = fp->initFileInfo(qstrTOstr(fileInfo.filePath()), qstrTOstr(folder),qstrTOstr(childpath));

	QVariant var = QVariant::fromValue((void*)data);

	QTableWidgetItem* item = new QTableWidgetItem(fileInfo.fileName(), 1000);
	if (!item) {
		return false;
	}
	item->setData(1, var);

	// 文件名称
	ui.tablewidget->setItem(row, ECOL_FILENAME, item);
	ui.tablewidget->item(row, ECOL_FILENAME)->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);  // 设置文字居左

	// 文件完成时间
	ui.tablewidget->setItem(row, ECOL_TIME, new QTableWidgetItem(""));
	ui.tablewidget->item(row, ECOL_TIME)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

	// 进度条的style设置
	QProgressBar* progressbar = new QProgressBar();
	if (!progressbar) {
		LOG_ERR("progressbar new fail!!!");
		return false;
	}
	progressbar->setStyleSheet(
		"QProgressBar {border: 1px solid grey;selection-background-color: rgb(193,210,240);"
		"border-radius:5px;"
		"text-align: center;"
		"font-size:15px;}"
		"QProgressBar::chunk {background-color: rgb(62,107,253);}"
	);

	progressbar->setRange(0, 100);
	ui.tablewidget->setCellWidget(row, ECOL_PRO, progressbar);

	// 文件状态
	ui.tablewidget->setItem(row, ECOL_STATUS, new QTableWidgetItem(statusName[data->_status]));
	ui.tablewidget->item(row, ECOL_STATUS)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);  // 设置文字居中

	// 文件传输速率
	ui.tablewidget->setItem(row, ECOL_SPEED, new QTableWidgetItem(""));
	ui.tablewidget->item(row, ECOL_SPEED)->setTextAlignment(Qt::AlignVCenter | Qt::AlignRight);     // 设置文字居右


	// 文件已完成大小/文件大小
	ui.tablewidget->setItem(row, ECOL_COMPLETED, new QTableWidgetItem(""));
	ui.tablewidget->item(row, ECOL_COMPLETED)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

	// 文件大小
	double size = fileInfo.size() * 1.0;
	QString strUnit(" B");
	if (fileInfo.size() > UNIT_SIZE) { // kb
		size = KB(fileInfo.size());
		strUnit = " KB";
		if (fileInfo.size() > UNIT_SIZE * UNIT_SIZE) {
			size = MB(fileInfo.size());
			strUnit = " MB";
			if (fileInfo.size() > UNIT_SIZE * UNIT_SIZE * UNIT_SIZE) {
				size = GB(fileInfo.size());
				strUnit = " GB";
			}
		}
	}

	QString filesize = tr("%1").arg(size, 0, 'f', 2) + strUnit;
	data->_filesize = filesize;
	ui.tablewidget->setItem(row, ECOL_SIZE, new TableWidgetItem(filesize));
	ui.tablewidget->item(row, ECOL_SIZE)->setTextAlignment(Qt::AlignVCenter | Qt::AlignRight);
	return true;
}

// 恢复数据插入
bool filetransfqt::recoverInsertRowInfo(Data& d, int row) {
	if (!fp->checkLocalExist(qstrTOstr(d._filepath))) {
		QString text = d._filepath + "\n本地文件不存在，可能移除或损坏";
		showHintLabel(this, text, 2000, 150, 60, "15px");
		return false;
	}
	
	ui.tablewidget->insertRow(row);

	// 初始化data
	Data* data = new Data;
	data->_name = d._name;
	data->_filepath = d._filepath;
	data->_folder = d._folder;
	data->_status = d._status;
	data->_bstatu = false;
	data->_status_prev = d._status_prev;
	data->_progress = d._progress;
	data->_host = configIni->value("host/host").toString() + configIni->value("upback/host").toString();
	data->_token = token;
	data->_dir = d._dir;       
	data->_dir_id = d._dir_id; 
	data->_speed = 0.0;
	data->_info = fp->initFileInfo(qstrTOstr(d._filepath), qstrTOstr(d._folder), qstrTOstr(d._dir));

	QVariant var = QVariant::fromValue((void*)data);

	QTableWidgetItem* item = new QTableWidgetItem(data->_name, 1000);
	if (!item) {
		return false;
	}
	item->setData(1, var);

	// 文件名称
	ui.tablewidget->setItem(row, ECOL_FILENAME, item);
	ui.tablewidget->item(row, ECOL_FILENAME)->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);  // 设置文字居左

	// 文件完成时间
	ui.tablewidget->setItem(row, ECOL_TIME, new QTableWidgetItem(""));
	ui.tablewidget->item(row, ECOL_TIME)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

	// 进度条的style设置
	QProgressBar* progressbar = new QProgressBar();
	if (!progressbar) {
		LOG_ERR("progressbar new fail!!!");
		return false;
	}
	progressbar->setStyleSheet(
		"QProgressBar {border: 1px solid grey;selection-background-color: rgb(193,210,240);"
		"border-radius:5px;"
		"text-align: center;"
		"font-size:15px;}"
		"QProgressBar::chunk {background-color: rgb(62,107,253);}"
	);

	progressbar->setRange(0, 100);
	ui.tablewidget->setCellWidget(row, ECOL_PRO, progressbar);

	// 文件状态
	ui.tablewidget->setItem(row, ECOL_STATUS, new QTableWidgetItem(statusName[NOTSTART]));
	ui.tablewidget->item(row, ECOL_STATUS)->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);  // 设置文字居中

	// 文件传输速率
	ui.tablewidget->setItem(row, ECOL_SPEED, new QTableWidgetItem(""));
	ui.tablewidget->item(row, ECOL_SPEED)->setTextAlignment(Qt::AlignVCenter | Qt::AlignRight);     // 设置文字居右


	// 文件已完成大小/文件大小
	ui.tablewidget->setItem(row, ECOL_COMPLETED, new QTableWidgetItem(""));
	ui.tablewidget->item(row, ECOL_COMPLETED)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

	// 文件大小
	data->_filesize = d._filesize;
	ui.tablewidget->setItem(row, ECOL_SIZE, new TableWidgetItem(d._filesize));
	ui.tablewidget->item(row, ECOL_SIZE)->setTextAlignment(Qt::AlignVCenter | Qt::AlignRight);

	return true;
}

// 解析DirectoryTree-Json
bool filetransfqt::getDirectoryTree() {
	QString hostkey = "dir/host";
	QString host = configIni->value(hostkey).toString();
	QString urlkey = "host/host";
	QString url = configIni->value(urlkey).toString();
	QString path = url + host;

	FtpProtocol http;
	string stdStr = http.getDirectoryTree(qstrTOstr(path), qstrTOstr(token));
	QString qtStr = strTOqstr_uft8(stdStr);               // strTOqstr() 中文乱码

	int code = -1;
	QJsonParseError json_error;
	QJsonDocument json_document = QJsonDocument::fromJson(qtStr.toStdString().data(), &json_error);
	if (!json_document.isNull() && (json_error.error == QJsonParseError::NoError)) {
		LOG_INFO("fromJson successed");
		if (json_document.isObject()) {
			QJsonObject json_object = json_document.object();
			if (json_object.contains("code")) {
				QJsonValue json_value = json_object.value("code");
				if (json_value.isDouble()) {
					code = json_value.toInt();
					qDebug() << endl;
					qDebug() << "code" << code;
				}
			}

			if (code == 200) {
				if (json_object.contains("msg")) {
					QJsonValue json_value = json_object.value("msg");
					if (json_value.isString()){
						QString msg = json_value.toString();
						qDebug() << "msg" << msg;
					
					}
				}

				if (json_object.contains("data")) {
					QJsonValue json_value = json_object.value("data");
					analysisJson(json_value);
				}
			}
			else {
				QString msg;
				if (json_object.contains("msg")) {
					QJsonValue json_value = json_object.value("msg");
					if (json_value.isString()) {
						msg = json_value.toString();
					}
				}
				LOG_ERR("Json obj code accident value:%d,%s", code, qstrTOstr(msg).c_str());
				return false;
			}
		}
	}
	else {
		// 转Json失败
		LOG_ERR("fromJson failed,the reason :%s",qstrTOstr(json_error.errorString()).c_str());
		return false;  
	}

	LOG_INFO("Obtain json info success");
	return true;
}

// 递归解析DirectoryTree Json
void filetransfqt::analysisJson(QJsonValue& json_value){
	if (json_value.isArray())	{
		QJsonArray json_array = json_value.toArray();
		int size = json_array.size();   
		for (int i = 0; i < json_array.size(); i++) {
			analysisJson(json_array.at(i));
		}
	}
	else if (json_value.isObject())	{
		int id = -1, pid = -1;
		QString name;

		QJsonObject json_obj = json_value.toObject();
		if (json_obj.contains("id")) {
			QJsonValue json_value_data = json_obj.value("id");
			if (json_value_data.isDouble()) {
				id = json_value_data.toInt();
			}
		}

		if (json_obj.contains("pid")) {
			QJsonValue json_value_data = json_obj.value("pid");
			if (json_value_data.isDouble()) {
				pid =json_value_data.toInt();
			}
		}

		if (json_obj.contains("name")) {
			QJsonValue json_value_data = json_obj.value("name");
			if (json_value_data.isString()) {
				name = json_value_data.toString();
			}
		}

		//emit signal_sendTreeNode(id, pid, name);
		createDirTree(id, pid, name);

		if (json_obj.contains("children")) {
			QJsonValue json_value = json_obj.value("children");
			if (json_value.isArray()) {
				QJsonArray json_array = json_value.toArray();
				int size = json_array.size();  
				for (int i = 0; i < json_array.size(); i++) {
					analysisJson(json_array.at(i));
				}
			}
		}
	}
}

// 通过文件全路径拿到当前行数
int filetransfqt::getRowOfFilename(string& filepath, int statu, bool flag){
	int res = -1;
	for (int row = 0; row < ui.tablewidget->rowCount(); row++) {
		QTableWidgetItem* it = ui.tablewidget->item(row, ECOL_FILENAME);
		if (!it) {
			return res;
		}
		QVariant var = it->data(1);
		Data* data = (Data*)var.value<void*>();
		if (!data) {
			return res;
		}

		if (flag && (statu & status[data->_status])) {
			if (strTOqstr(filepath) == data->_filepath) {
				res = row;
				break;
			}
		}
		else if(!flag /*&& statu == data->_status*/) {              // 为了复用，
			if (filepath == data->_info->remoteFilePath) {
				res = row;
				break;
			}
		}
	}
	return res;
}

// 创建目录树
void filetransfqt::createDirTree(int id, int pid, QString name){
	qDebug() << id << pid << name;
	QString npid = QString::number(pid);
	QString nid = QString::number(id);

	if (pid == 0) {
		QString npid = QString::number(pid);
		addTreeRoot(name, nid);

	}
	else {
		for (int i = 0; ; i++) {
			QTreeWidgetItem* parent = ui.dirtreewidget->topLevelItem(i);
			if (!parent) {
				break;
			}

			addChildNode(parent, name, nid, npid);

			QString sid = ui.dirtreewidget->topLevelItem(i)->text(1);
			if (sid == npid) {
				addTreeNode(parent, name, nid);
				break;
			}
		}
	}
}

// ui删除一个
void filetransfqt::deleteData(int row){
	QTableWidgetItem* it = ui.tablewidget->item(row, ECOL_FILENAME);
	if (!it) {
		return;
	}
	QVariant var = it->data(1);
	Data* data = (Data*)var.value<void*>();
	if (!data) {
		return;
	}
	delete data;
	data = nullptr;

	delete ui.tablewidget->cellWidget(row, ECOL_PRO);  
	delete ui.tablewidget->item(row, ECOL_FILENAME);
	delete ui.tablewidget->item(row, ECOL_STATUS);
	delete ui.tablewidget->item(row, ECOL_SIZE);
	delete ui.tablewidget->item(row, ECOL_TIME);
	delete ui.tablewidget->item(row, ECOL_COMPLETED);
	delete ui.tablewidget->item(row, ECOL_SPEED);
	
	ui.tablewidget->removeRow(row);
}






