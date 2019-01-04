#include "mainwindow.h"
#include "ui_mainwindow.h"
#include<QDebug>
#include<QFileDialog>
#include <QFile>
#include <QTextStream>
#include <ros/package.h>
MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent),
  ui(new Ui::MainWindow)
{
  firstLoad=true;
  ui->setupUi(this);
  setUpHighlighter();
  //init status bar
  ui->outputText->parentWindow=this;
  ui->statusBar->showMessage(tr("Ready"));
  //--------init toolbar------------
  //ui->statusBar->setStyleSheet("QStatusBar{background:rgb(50,50,50);}");
  ui->mainToolBar->setMovable(false);
  ui->mainToolBar->setStyleSheet("QToolButton:hover {background-color:darkgray} QToolBar {background: rgb(179, 204, 204);border: none;}");
  //--------------------------------

  runIcon.addPixmap(QPixmap(":/img/run.png"));
  stopIcon.addPixmap(QPixmap(":/img/stop.png"));
  errorIcon.addPixmap(QPixmap(":/img/error.png"));
  RerrorIcon.addPixmap(QPixmap(":/img/error_red.png"));


  QPalette windowPalette=this->palette();
  windowPalette.setColor(QPalette::Active,QPalette::Window,QColor(82,82,82));
  windowPalette.setColor(QPalette::Inactive,QPalette::Window,QColor(82,82,82));
  this->setPalette(windowPalette);
  //--------------------------------
  initFileData();
  this->updateRobot();
  connect(ui->actionNewFile,SIGNAL(triggered(bool)),this,SLOT(newFile()));
  connect(ui->actionOpen,SIGNAL(triggered(bool)),this,SLOT(openFile()));
  connect(ui->actionSave_File,SIGNAL(triggered(bool)),this,SLOT(saveFile()));
  connect(ui->actionUndo,SIGNAL(triggered(bool)),this,SLOT(undo()));
  connect(ui->actionRedo,SIGNAL(triggered(bool)),this,SLOT(redo()));
  connect(ui->editor,SIGNAL(textChanged()),this,SLOT(changeSaveState()));
  connect(ui->actionRun,SIGNAL(triggered(bool)),this,SLOT(run()));
//  connect(&process,SIGNAL(finished(int)),this,SLOT(runFinished(int)));
//  connect(&process,SIGNAL(readyReadStandardOutput()),this,SLOT(updateOutput()));
//  connect(&process,SIGNAL(readyReadStandardError()),this,SLOT(updateError()));
  connect(ui->actionAbout,SIGNAL(triggered(bool)),this,SLOT(about()));
  fileSaved=true;
//  msg1.points.resize(1);
//  msg1.points[0].positions.resize(6);
//  msg1.points[1].positions.resize(6);

  msgolder.points.resize(1);
  msgolder.points[0].positions.resize(6);
   joint_pub =       nh_.advertise<trajectory_msgs::JointTrajectory>("set_joint_trajectory", 10);
   joint_sub_limit = nh_.subscribe("/joint_limits",10,&MainWindow::jointsizeCallback, this);

   spinner = boost::shared_ptr<ros::AsyncSpinner>(new ros::AsyncSpinner(1));
   spinner->start();
//   boost::thread* publisher_thread_;
   isloop=false;
   limit.data.resize(12);
   limit.data.push_back(0);
   publisher_thread_ = new boost::thread(boost::bind(&MainWindow::runloop, this));



}

MainWindow::~MainWindow()
{
  if(publisher_thread_ != NULL)
    {
      publisher_thread_->interrupt();
      publisher_thread_->join();

      delete publisher_thread_;
    }
  delete ui;
}
void MainWindow::setUpHighlighter(){
  QFont font;
  font.setFamily("Courier");
  font.setFixedPitch(true);
  font.setPointSize(14);
  font.setBold(true);
  ui->editor->setFont(font);
  ui->editor->setTabStopWidth(fontMetrics().width(QLatin1Char('9'))*4);
  highlighter=new Highlighter(ui->editor->document());
}

void MainWindow::resizeEvent(QResizeEvent *event){
  QMainWindow::resizeEvent(event);
  ui->editor->setGeometry(10,0,width()-20,height()-ui->statusBar->height()-ui->mainToolBar->height()-80-15);
  ui->outputText->setGeometry(10,ui->editor->height()+10,this->width()-20,80);
}
void MainWindow::initFileData(){
  fileName=tr("Untitled.rrun");
  filePath=QString::fromUtf8( ros::package::getPath("interpreter_gui").c_str()) + "/Script/Untitled.rrun"; //QDir::homePath()+"/ros_qtc_plugin/src/interpreter_gui/Script";//tr("/home/yesser/ros_qtc_plugin/src/interpreter_gui"); //QCoreApplication::applicationDirPath()
  fileSaved=true;
  isRunning=false;
}
void MainWindow::undo(){
  ui->editor->undo();
}
void MainWindow::redo(){
  ui->editor->redo();
}
void MainWindow::saveFile(){
  QString savePath=QFileDialog::getSaveFileName(this,tr("Elija la ruta y nombre del archivo a guardar"),filePath,tr("Rrun File(*.rrun)"));
  if(!savePath.isEmpty()){
      QFile out(savePath);
      out.open(QIODevice::WriteOnly|QIODevice::Text);
      QTextStream str(&out);
      str<<ui->editor->toPlainText();
      out.close();
      fileSaved=true;
      QRegularExpression re(tr("(?<=\\/)\\w+\\.rrun"));
      fileName=re.match(savePath).captured();
      filePath=savePath;
      this->setWindowTitle(tr("Robot Script Editor - ")+fileName);
    }
  ui->actionError_Datos->setIcon(errorIcon);

}
void MainWindow::newFile(){
  MainWindow *newWindow=new MainWindow();
  QRect newPos=this->geometry();
  newWindow->setGeometry(newPos.x()+10,newPos.y()+10,newPos.width(),newPos.height());
  newWindow->show();
  ui->actionError_Datos->setIcon(errorIcon);
}
void MainWindow::openFile(){
  if(!fileSaved){
      if(QMessageBox::Save==QMessageBox::question(this,tr("Archivo no guardado"),tr("Guardar Archivo Actual"),QMessageBox::Save,QMessageBox::Cancel))
        saveFile();
    }
  QString openPath=QFileDialog::getOpenFileName(this,tr("Seleccione el archivo para abrir"),filePath,tr("Rrun File(*.rrun)"));
  if(!openPath.isEmpty()){
      QFile in(openPath);
      in.open(QIODevice::ReadOnly|QIODevice::Text);
      QTextStream str(&in);
      ui->editor->setPlainText(str.readAll());
      QRegularExpression re(tr("(?<=\\/)\\w+\\.rrun"));
      fileName=re.match(openPath).captured();
      this->setWindowTitle(tr("Robot Script Editor - ")+fileName);
      filePath=openPath;
      fileSaved=true;
    }
  ui->actionError_Datos->setIcon(errorIcon);
}
void MainWindow::run(){
  //    trajectory_point1.positions.clear();

  trajectory_msgs::JointTrajectory msg;
  msg.points.resize(1);
  msg.points[0].positions.resize(6);
  isloop =false;
  std::cout<< "step1" << std::endl;

  int it=0;
//       for (int j = 0; j < 6; j++) {
       msg1.points.clear();
       msg1.joint_names.clear();
         msg1.points.resize(1);
         msg1.points[0].positions.resize(6);
//       } //for
//       joint_pub.publish(msg1) ; //publish nodo


  if(isRunning){
//      process.terminate();
      ui->actionRun->setIcon(runIcon);
      return;
    }
  if(!fileSaved){
//      if(QMessageBox::Save==QMessageBox::question(this,tr("Archivo no guardado"),tr("Guardar Archivo Actual？"),QMessageBox::Save,QMessageBox::Cancel))
//        saveFile();
    QFile out(filePath);
    out.open(QIODevice::WriteOnly|QIODevice::Text);
    QTextStream str(&out);
    str<<ui->editor->toPlainText();
    out.close();
    fileSaved=true;

    }
  if(fileSaved){
    //if(process!=nullptr)delete process;
    isRunning=true;
    ui->statusBar->showMessage(tr("Programa En Ejecucion..."));
    ui->outputText->clear();
    output.clear();
    error.clear();
    QString buildPath;
    QRegularExpression re(tr(".*(?=\\.rrun)"));
    buildPath=re.match(filePath).captured();
//    qDebug()<<filePath;
//    process.start("bash", QStringList() << "-c" << QString(tr("g++ ")+filePath+tr(" -o ")+buildPath+tr(";")+buildPath));
//    process.waitForStarted();

    ui->outputText->setFocus();
    ui->actionRun->setIcon(stopIcon);

    std::ifstream streamcount(filePath.toStdString()); // OR "/home/yesser/ros_qtc_plugin/src/interpreter_gui/Script/firstScript.rrun"

    int points = std::count(std::istreambuf_iterator<char>(streamcount),std::istreambuf_iterator<char>(),'\n');
    std::cout<< "\n points in trajectory " << points << std::endl;

    std::ifstream stream(filePath.toStdString());
    if(stream.good()){
      std::string linea;
      while(!stream.eof()&&isRunning){
        linea.clear();
        std::getline(stream, linea);
        ROS_INFO("Proceso %s",linea.c_str());
        it++;
        msg = this->comandos(linea,it);
      }

      std::cout << "\n point = " << msg.points.size() << " " << msg  <<std::endl;
      joint_pub.publish(msg) ; //publish nodo
    }

    this->runFinished(0);


//    if (!isloop){
//    }

   } //file save
}

void MainWindow::jointsizeCallback(const std_msgs::Float32MultiArray::ConstPtr &msglimit) //Valores de los limtes de los joints
{
  std::cout<<"DATAOK"<< std::endl;

//  limit.data.resize(12);

    limit = *msglimit;
    for (int i =0; i<12;i++)
    {
  std::cout<< limit.data[i]<< std::endl;
    }
}


trajectory_msgs::JointTrajectory MainWindow::comandos(std::string &comando, int &ii){
  int i =ii;
  std::vector<double> jointvalues(6);
  trajectory_msgs::JointTrajectoryPoint comandoP;
  comandoP.positions.resize(6);
  comandoP.velocities.resize(6);


 //Full Zero (Core Dumped)
//      for (int j = 0; j < 6; j++) {
//      msg1.points[0].positions[j] = 0.00;
//      } //for


  std::cout << "\n I have iterator " << i <<std::endl;

  std::vector<std::string> partes=split(comando, ' ');

  if(partes.size()==0){
    std::cout<< "Line empty"<< std::endl;
//    msg1.points[0].positions[0] = 0.00;
      }else{
        switch(partes[0][6]){
          case '1':
            if(partes.size()>1){
              std::cout << "I have it 1 " << partes[2]  <<std::endl;
              this->updateOutput(partes[2]);

              comandoP.positions[0] = std::stod(partes[2]);
              comandoP.velocities[0] = std::stod(partes[4]);
              comandoP.velocities[1] = std::stod(partes[4]);
              comandoP.velocities[2] = std::stod(partes[4]);
              comandoP.velocities[3] = std::stod(partes[4]);
              comandoP.velocities[4] = std::stod(partes[4]);
              comandoP.velocities[5] = std::stod(partes[4]);


              if(comandoP.positions[0]>limit.data[0] && comandoP.positions[0]<limit.data[6] ){
                std::cout<< "is ok for now"<< std::endl;
                comandoP.positions[1] = msg1.points[i-1].positions[1];
                comandoP.positions[2] = msg1.points[i-1].positions[2];
                comandoP.positions[3] = msg1.points[i-1].positions[3];
                comandoP.positions[4] = msg1.points[i-1].positions[4];
                comandoP.positions[5] = msg1.points[i-1].positions[5];
                msg1.joint_names.push_back("joint_1");
                msg1.points.push_back(comandoP);
              }
              else {
                comandoP.positions[0] = 0.00;
                comandoP.velocities[0] = 0.00;
                msg1.joint_names.push_back("joint_1");
                msg1.points.push_back(comandoP);
                this->updateError();
                   }
            }
            break;
          case '2':
            if(partes.size()>1){
              std::cout << "I have it  2 " << partes[2]  <<std::endl;
              this->updateOutput(partes[2]);

              comandoP.positions[1] = std::stod(partes[2]);
              comandoP.velocities[0] = std::stod(partes[4]);
              comandoP.velocities[1] = std::stod(partes[4]);
              comandoP.velocities[2] = std::stod(partes[4]);
              comandoP.velocities[3] = std::stod(partes[4]);
              comandoP.velocities[4] = std::stod(partes[4]);
              comandoP.velocities[5] = std::stod(partes[4]);

//              msg1.points[i-1].positions[1]. = std::stod(partes[1]);

              if(comandoP.positions[1]>limit.data[1] && comandoP.positions[1]<limit.data[7] ){
                std::cout<< "is ok for now"<< std::endl;
                comandoP.positions[0] = msg1.points[i-1].positions[0];
                comandoP.positions[2] = msg1.points[i-1].positions[2];
                comandoP.positions[3] = msg1.points[i-1].positions[3];
                comandoP.positions[4] = msg1.points[i-1].positions[4];
                comandoP.positions[5] = msg1.points[i-1].positions[5];
                msg1.joint_names.push_back("joint_2");
                msg1.points.push_back(comandoP);
              }
              else {
                comandoP.positions[1] = 0.00;
                comandoP.velocities[0] = 0.00;
                msg1.joint_names.push_back("joint_2");
                msg1.points.push_back(comandoP);
//                msg1.points[i-1].positions[1] = 0.00;
                this->updateError();
                   }
            }
            break;

        case '3':
          if(partes.size()>1){
            std::cout << "I have it  3 " << partes[1]  <<std::endl;
            this->updateOutput(partes[1]);

            comandoP.positions[2] = std::stod(partes[2]);
            comandoP.velocities[0] = std::stod(partes[4]);
            comandoP.velocities[1] = std::stod(partes[4]);
            comandoP.velocities[2] = std::stod(partes[4]);
            comandoP.velocities[3] = std::stod(partes[4]);
            comandoP.velocities[4] = std::stod(partes[4]);
            comandoP.velocities[5] = std::stod(partes[4]);

            if(comandoP.positions[2]>limit.data[2] && comandoP.positions[2]<limit.data[8] ){
              std::cout<< "is ok for now"<< std::endl;
              comandoP.positions[0] = msg1.points[i-1].positions[0];
              comandoP.positions[1] = msg1.points[i-1].positions[1];
              comandoP.positions[3] = msg1.points[i-1].positions[3];
              comandoP.positions[4] = msg1.points[i-1].positions[4];
              comandoP.positions[5] = msg1.points[i-1].positions[5];
              msg1.joint_names.push_back("joint_3");
              msg1.points.push_back(comandoP);
              }
            else {
              comandoP.positions[2] = 0.00;
              comandoP.velocities[0] = 0.00;
              msg1.joint_names.push_back("joint_3");
              msg1.points.push_back(comandoP);
              this->updateError();
                 }
          }
          break;

        case '4':
          if(partes.size()>1){
            std::cout << "I have it  4 " << partes[1]  <<std::endl;
            this->updateOutput(partes[1]);


            comandoP.positions[3] = std::stod(partes[2]);
            comandoP.velocities[0] = std::stod(partes[4]);
            comandoP.velocities[1] = std::stod(partes[4]);
            comandoP.velocities[2] = std::stod(partes[4]);
            comandoP.velocities[3] = std::stod(partes[4]);
            comandoP.velocities[4] = std::stod(partes[4]);
            comandoP.velocities[5] = std::stod(partes[4]);

            if(comandoP.positions[3]>limit.data[3] && comandoP.positions[3]<limit.data[9] ){
              std::cout<< "is ok for now"<< std::endl;
              comandoP.positions[0] = msg1.points[i-1].positions[0];
              comandoP.positions[1] = msg1.points[i-1].positions[1];
              comandoP.positions[2] = msg1.points[i-1].positions[2];
              comandoP.positions[4] = msg1.points[i-1].positions[4];
              comandoP.positions[5] = msg1.points[i-1].positions[5];
              msg1.joint_names.push_back("joint_4");
              msg1.points.push_back(comandoP);
              }
            else {
              comandoP.positions[3] = 0.00;
              comandoP.velocities[0] = 0.00;
              msg1.joint_names.push_back("joint_4");
              msg1.points.push_back(comandoP);
              this->updateError();
                 }
          }
          break;

        case '5':
          if(partes.size()>1){
            std::cout << "I have it  5 " << partes[1]  <<std::endl;
            this->updateOutput(partes[1]);

            comandoP.positions[4] = std::stod(partes[2]);
            comandoP.velocities[0] = std::stod(partes[4]);
            comandoP.velocities[1] = std::stod(partes[4]);
            comandoP.velocities[2] = std::stod(partes[4]);
            comandoP.velocities[3] = std::stod(partes[4]);
            comandoP.velocities[4] = std::stod(partes[4]);
            comandoP.velocities[5] = std::stod(partes[4]);

            if(comandoP.positions[4]>limit.data[4] && comandoP.positions[4]<limit.data[10] ){
              std::cout<< "is ok for now"<< std::endl;
              comandoP.positions[0] = msg1.points[i-1].positions[0];
              comandoP.positions[1] = msg1.points[i-1].positions[1];
              comandoP.positions[2] = msg1.points[i-1].positions[2];
              comandoP.positions[3] = msg1.points[i-1].positions[3];
              comandoP.positions[5] = msg1.points[i-1].positions[5];
              msg1.joint_names.push_back("joint_5");
              msg1.points.push_back(comandoP);
              }
            else {
              comandoP.positions[4] = 0.00;
              comandoP.velocities[0] = 0.00;
              msg1.joint_names.push_back("joint_5");
              msg1.points.push_back(comandoP);
              this->updateError();
                 }
          }
          break;

        case '6':
          if(partes.size()>1){
            std::cout << "I have it  6 " << partes[1]  <<std::endl;
            this->updateOutput(partes[1]);

            comandoP.positions[5] = std::stod(partes[2]);
            comandoP.velocities[0] = std::stod(partes[4]);
            comandoP.velocities[1] = std::stod(partes[4]);
            comandoP.velocities[2] = std::stod(partes[4]);
            comandoP.velocities[3] = std::stod(partes[4]);
            comandoP.velocities[4] = std::stod(partes[4]);
            comandoP.velocities[5] = std::stod(partes[4]);

            if(comandoP.positions[5]>limit.data[5] && comandoP.positions[5]<limit.data[11] ){
              std::cout<< "is ok for now"<< std::endl;
              comandoP.positions[0] = msg1.points[i-1].positions[0];
              comandoP.positions[1] = msg1.points[i-1].positions[1];
              comandoP.positions[2] = msg1.points[i-1].positions[2];
              comandoP.positions[3] = msg1.points[i-1].positions[3];
              comandoP.positions[4] = msg1.points[i-1].positions[4];
              msg1.joint_names.push_back("joint_6");
              msg1.points.push_back(comandoP);
              }
            else {
              comandoP.positions[5] = 0.00;
              comandoP.velocities[0] = 0.00;
              msg1.joint_names.push_back("joint_6");
              msg1.points.push_back(comandoP);
              this->updateError();
                 }
          }
          break;

          case 'm':
            //m union posicion_radianes
            //Mueve la unión hasta la posición en radianes indicada
            if(partes.size()>2){
//              robot->mover(partes[1],std::stod(partes[2]));
            }
            break;

          case 'w':
            //w [union]
            //Espera hasta que la unión halla llegado a su destino. Si no se indica la unión se espera hata terminar todas
            ROS_INFO("eseparndo");
            break;
          case 's':
            //s milisegundos
            //Duerme los milisegundos indicados.

            //de momento nos dormimos durante el tiempo indicado.
            std::string tiempo(partes[1]);
            ROS_INFO("durmiendo %d",std::stoi(tiempo));
            boost::this_thread::sleep(boost::posix_time::milliseconds(std::stoi(tiempo)));
            //usleep(std::stoi(tiempo));
            ROS_INFO("despierto");
            break;/**/
        }
      }

  if(partes.size()==0){
    std::cout<< "Line empty"<< std::endl;
  }else{
        if(partes[0]== std::string("loop()"))
        {
          std::cout<< "I have loop()"<< std::endl;
          //msg1.points[0].positions[7] = 0.00;
          isloop =true;
          //    this->runloop();
          this->updateOutput(partes[0]);
        }
        else {
          std::cout<< "I don't have loop()"<< std::endl;
             }
          }
//  for (int k = 0; k < 6; k++) {
//   msg1.points[0].positions[k] =jointvalues[k]; //array [i]
//                              }
//  msgolder = msg1;
return msg1;
}

void MainWindow::runloop(){
  boost::mutex::scoped_lock state_pub_lock(state_pub_mutex_);
  ros::Rate loop_rate(10);

//  msgolder.points.resize(1);
//  msgolder.points[0].positions.resize(6);

  while (true){

//    for (int j = 0; j < 6; j++) {
//    msgolder.points[0].positions[j] = 0.0;
//    joint_pub.publish(msgolder);
//    } //for
    if(isloop)
    {
    std::cout<< "I'm alive"<< std::endl;
    joint_pub.publish(msg1);
    }
//  for (int i = 0; i < 6; i++) {
//  msgolder.points[0].positions[i] = msg1.points[0].positions[i];
//  joint_pub.publish(msgolder);
//  } //for
    try {
      boost::this_thread::interruption_point();
    } catch(const boost::thread_interrupted& o) {
      break; // quit the thread's loop
    }

  loop_rate.sleep();

  }//while
}


std::vector<std::string> MainWindow::split(const std::string &c, char d){
  std::vector<std::string> resultado;

  std::stringstream cs(c);
  std::string parte;
  while(std::getline(cs, parte, d)){
    resultado.push_back(parte);
  }

  return resultado;
}

void MainWindow::runFinished(int code){
  ui->actionRun->setIcon(runIcon);
  isRunning=false;
  qDebug()<<tr("exit code=")<<code;
  ui->statusBar->showMessage(tr("Ready"));

}
void MainWindow::updateOutput(std::string &info)
{
  output=QString::fromLocal8Bit(info.c_str());
  //ui->outputText->setPlainText(output+tr("\n")+error);
  ui->outputText->setPlainText(ui->outputText->toPlainText()+tr("Ejecutando Movimientos ")+output+tr("... \n"));//+tr("\n"));
}
void MainWindow::updateError(){
  error=QString("Valores Fuera del espacio de trabajo \n Regresando Robot a su estado Inicial");
//  //ui->outputText->setPlainText(output+tr("\n")+error);
  ui->outputText->setPlainText(ui->outputText->toPlainText()+error);//+tr("\n"));
//  ui->actionError_Datos->isEnabled();
  ui->actionError_Datos->setIcon(RerrorIcon);
  isRunning=false;
}

void MainWindow::updateRobot(){
  error=QString("Por Favor cargue un modelo de Robot a ROS");
  ui->outputText->setPlainText(ui->outputText->toPlainText()+error);//+tr("\n"));
  ui->actionError_Datos->setIcon(errorIcon);
  isRunning=false;
}

void MainWindow::inputData(QString data){
//  if(isRunning)process.write(data.toLocal8Bit());
}
void MainWindow::closeEvent(QCloseEvent *event){
  if(!fileSaved){
      if(QMessageBox::Save==QMessageBox::question(this,tr("¿Salir sin guardar?"),tr("El archivo actual no se ha guardado"),QMessageBox::Save,QMessageBox::Cancel))
        saveFile();
      fileSaved=true;
    }
}
void MainWindow::about(){
  QMessageBox::information(this,tr("About"),tr(" Yeser M. v1.1 \n Universidad Nacional de Ingenieria \nmyalfredo03@ieee.org"),QMessageBox::Ok);
}

//CODE Heuristic
//    msg1.points.resize(points+1);
//    for (std::size_t k=0; k==points; ++k){
//      if(true){
//    msg1.points[k].positions.resize(6);
//    }
//    }
//msg1.points[i-1].positions[2] = std::stod(partes[1]);
