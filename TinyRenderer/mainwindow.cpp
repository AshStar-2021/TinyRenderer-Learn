#include "mainwindow.h"
#include "ui_mainwindow.h"

//Model
Model *model = NULL;
std::string filePath;


//Image
const int imgWidth  = 800;
const int imgHeight = 800;
const int depth  = 255;
TGAImage image(imgWidth, imgHeight, TGAImage::RGB);
TGAImage zbuffer(imgWidth, imgHeight, TGAImage::GRAYSCALE);
QGraphicsScene* scene;
QImage img;


//camera相关

Vec3f eye(0,0,1.5f);       //摄像机位置
Vec3f focuPosition(0,0,0);  //焦点位置
Vec3f up(0,1,0);
Vec3f cameraFront(0,0,-1.5f);
//Vec3f light_dir(0,1,1); // define light_dir
Vec3f light_dir(0.8,0,1); // define light_dir
float rotationSpeed = 0.0008f;
float yaw = 90.0f,pitch = 0.0f;
double eyeDistance = 1.5;
//shader相关
IShader *shader;


//update相关
int frameCount = 0;
QString debugText;
//输入相关
int mousePositionX,mousePositionY;
float scrollSpeed = 0.05f;
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    InitMainwindow();

    QTimer *timer = new QTimer(this);
    connect(timer,SIGNAL(timeout()) ,this,SLOT(update()));
    timer->start(50);
}

MainWindow::~MainWindow()
{
    delete ui;
}

//不对法向量进行插值，法向量来源于三角形边的叉积
class FlatShader : public IShader {
public:
    //三个点的信息
    mat<3, 3, float> varying_tri;

    virtual ~FlatShader() {}

    virtual Vec4f vertex(int iface, int nthvert) {
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));
        gl_Vertex = Projection * ModelView * gl_Vertex;
        varying_tri.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3]));
        gl_Vertex = Viewport * gl_Vertex;
        return gl_Vertex;
    }

    virtual bool fragment(Vec3f bar, TGAColor& color) {
        Vec3f n = cross(varying_tri.col(1) - varying_tri.col(0), varying_tri.col(2) - varying_tri.col(0)).normalize();
        float intensity = n * light_dir;
        color = TGAColor(255, 255, 255) * intensity;
        return false;
    }
};

//高洛德着色器
class GouraudShader : public IShader {
public:
    //顶点着色器会将数据写入varying_intensity
    //片元着色器从varying_intensity中读取数据
    Vec3f varying_intensity;
    mat<2, 3, float> varying_uv;
    //接受两个变量，(面序号，顶点序号)
    virtual Vec4f vertex(int iface, int nthvert) {
        //根据面序号和顶点序号读取模型对应顶点，并扩展为4维
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        //变换顶点坐标到屏幕坐标（视角矩阵*投影矩阵*变换矩阵*v）
        mat<4, 4, float> uniform_M = Projection * ModelView;
        mat<4, 4, float> uniform_MIT = ModelView.invert_transpose();
        gl_Vertex = Viewport* uniform_M *gl_Vertex;
        //计算光照强度（顶点法向量*光照方向）
        Vec3f normal = proj<3>(embed<4>(model->normal(iface, nthvert))).normalize();
        varying_intensity[nthvert] = std::max(0.f, model->normal(iface, nthvert) *light_dir); // get diffuse lighting intensity
        return gl_Vertex;
    }
    //根据传入的质心坐标，颜色，以及varying_intensity计算出当前像素的颜色
    virtual bool fragment(Vec3f bar, TGAColor &color) {
        Vec2f uv = varying_uv * bar;
        TGAColor c = model->diffuse(uv);
        float intensity = varying_intensity*bar;
        color = c*intensity;
        return false;
    }
};

//Phong氏着色
class PhongShader : public IShader {
public:
    mat<2, 3, float> varying_uv;  // same as above
    mat<4, 4, float> uniform_M = Projection * ModelView;
    mat<4, 4, float> uniform_MIT = ModelView.invert_transpose();
    virtual Vec4f vertex(int iface, int nthvert) {
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert)); // read the vertex from .obj file
        return Viewport * Projection * ModelView * gl_Vertex; // transform it to screen coordinates
    }
    virtual bool fragment(Vec3f bar, TGAColor& color) {
        Vec2f uv = varying_uv * bar;
        Vec3f n = proj<3>(uniform_MIT * embed<4>(model->normal(uv))).normalize();
        Vec3f l = proj<3>(uniform_M * embed<4>(light_dir)).normalize();
        Vec3f r = (n * (n * l * 2.f) - l).normalize();   // reflected light
        float spec = pow(std::max(r.z, 0.0f), model->specular(uv));
        float diff = std::max(0.f, n * l);
        TGAColor c = model->diffuse(uv);
        color = c;
        for (int i = 0; i < 3; i++) color[i] = std::min<float>(5 + c[i] * (diff + .6 * spec), 255);
        return false;
    }
};

//将一定阈值内的光照强度给替换为一种
class ToonShader : public IShader {
public:
    mat<3, 3, float> varying_tri;
    Vec3f          varying_ity;
    mat<2, 3, float> varying_uv;
    virtual ~ToonShader() {}

    virtual Vec4f vertex(int iface, int nthvert) {
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));
        gl_Vertex = Projection * ModelView * gl_Vertex;
        //varying_tri.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3]));

        varying_ity[nthvert] = model->normal(iface, nthvert) * light_dir;
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        gl_Vertex = Viewport * gl_Vertex;
        return gl_Vertex;
    }

    virtual bool fragment(Vec3f bar, TGAColor& color) {
        Vec2f uv = varying_uv * bar;
        TGAColor c = model->diffuse(uv);

        float intensity = varying_ity * bar;
//        if (intensity > .85) intensity = 1;
//        else if (intensity > .60) intensity = .80f;
//        else if (intensity > .45) intensity = .60f;
//        else if (intensity > .30) intensity = .45f;
//        else if (intensity > .15) intensity = .30f;
        if(intensity > .65) intensity = .80f;
        else intensity = .40f;

        //color = TGAColor(255, 155, 0) * intensity;
        color = c * intensity;
        return false;
    }
};

void MainWindow::MainRender()
{
    //1.渲染
    //初始化image和zbuffer
    image.clear();
    zbuffer.clear();

    //以模型面作为循环控制量
    for (int i=0; i<model->nfaces(); i++) {
        Vec4f screen_coords[3];
        for (int j=0; j<3; j++) {
            //通过顶点着色器读取模型顶点
            //变换顶点坐标到屏幕坐标（视角矩阵*投影矩阵*变换矩阵*v） ***其实并不是真正的屏幕坐标，因为没有除以最后一个分量
            //计算光照强度
            screen_coords[j] = shader->vertex(i, j);
        }
        //遍历完3个顶点，一个三角形光栅化完成
        //绘制三角形，triangle内部通过片元着色器对三角形着色
        triangle(screen_coords, shader, image, zbuffer);
    }
    image.flip_vertically();


    //2.TGAImage转化为QImage
    img = QImage(image.buffer(),imgWidth,imgHeight,QImage::Format_BGR888);
    scene->clear();
    scene->addPixmap(QPixmap::fromImage(img));

    //zbuffer.flip_vertically();
    //QImage zbufferImg = QImage(zbuffer.buffer(),imgWidth,imgHeight,QImage::Format_Grayscale8);
    //QGraphicsScene* scene2 = new QGraphicsScene;
    //scene2->addPixmap( QPixmap::fromImage(zbufferImg));
    //ui->zbufferGView->setScene(scene2);
    //ui->zbufferGView->setSceneRect(0,0,imgWidth,imgHeight);

    //3.析构
}


void MainWindow::InitMainwindow()
{
    std::cerr << "开始渲染";
    ui->setupUi(this);
    setWindowState(Qt::WindowMaximized);

    //装载模型
    std::cerr << QDir::currentPath().toStdString();
    filePath = "D:/compiler/qt/workspace/TinyRenderer/obj/african_head/african_head.obj";
    //filePath = "D:/compiler/qt/workspace/TinyRenderer/obj/Haruko/Haruko.obj";
    //filePath = "obj/african_head.obj";
    model = new Model(filePath.c_str());


    //渲染相关
    //初始化变换矩阵，投影矩阵，视角矩阵
    Vec3f front(cos(yaw)*cos(pitch) ,sin(pitch), sin(yaw)*cos(pitch));
    eye = front.normalize() *eyeDistance;
    lookat(eye, Vec3f(0,0,0), up);
    projection(-1.f/(eye-focuPosition).norm());

    viewport(imgWidth / 8, imgHeight / 8, imgWidth * 3 / 4, imgHeight * 3 / 4);

    //shader = new FlatShader();
    shader = new GouraudShader();
    //shader = new PhongShader();
    //shader = new ToonShader();


    scene = new QGraphicsScene;
    ui->graphicsView->setScene(scene);
    ui->graphicsView->setSceneRect(0,0,imgWidth,imgHeight);
    light_dir.normalize();
    update();
}


//UI
void MainWindow::on_actionOpen_triggered()
{
    QString filename = QFileDialog::getOpenFileName(
                this,
                tr("Open File"),
                "D:/compiler/qt/workspace/TinyRenderer/obj",
                "Model file(*.obj)"
                );

    filePath.clear();
    filePath = filename.toStdString();
    model = new Model(filePath.c_str());
}

void MainWindow::on_lightX_valueChanged(int value)
{
    light_dir.x = value*0.1;
}

void MainWindow::on_lightY_valueChanged(int value)
{
    light_dir.y = value*0.1;
}

void MainWindow::on_lightZ_valueChanged(int value)
{
    light_dir.z = value*0.1;
}


void MainWindow::on_comboBox_currentIndexChanged(int index)
{
    //删除后新建shader
    delete shader;
    switch(index)
    {
        case 0:
            shader = new FlatShader();
            break;
        case 1:
            shader = new GouraudShader();
            break;
        case 2:
            shader = new PhongShader();
            break;
        case 3:
            shader = new ToonShader();
    }
}

//mouseEvent///////////////////////////////////////////////////////////////////
void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::RightButton)
    {
        mousePositionX = event->pos().x();
        mousePositionY = event->pos().y();
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if(event->buttons() == Qt::RightButton)
    {
        int tempX = event->pos().x();
        int tempY = event->pos().y();
        int offsetX = tempX - mousePositionX;
        int offsetY = tempY - mousePositionY;
        mousePositionX = tempX;
        mousePositionY = tempY;
        debugText = "mouseMove:" + QString::number(offsetX,10) + "," + QString::number(offsetY,10);
        ui->DebugLog->setText(debugText);

        //重新计算相机矩阵
        yaw += offsetX * rotationSpeed;
        pitch += offsetY * rotationSpeed;

        // make sure that when pitch is out of bounds, screen doesn't get flipped
        if (pitch > 90.0f)
            pitch = 90.0f;
        if (pitch < -90.0f)
            pitch = -90.0f;

        Vec3f front(cos(yaw)*cos(pitch) ,sin(pitch), sin(yaw)*cos(pitch));
        eye = front.normalize() *eyeDistance;
        lookat(eye, Vec3f(0,0,0), up);
    }
}

void MainWindow::wheelEvent(QWheelEvent *event)
{
    eyeDistance += event->delta() * scrollSpeed;
    if(eyeDistance < 0.5f)
        eyeDistance = 0.5f;
    //debugText = "eyeDistance:" + QString::number(eyeDistance,'g',10);
    //ui->DebugLog->setText(debugText);

    eye = eye.normalize() * eyeDistance;
    lookat(eye, Vec3f(0,0,0), up);
}

void MainWindow::update()
{
    frameCount++;
    std::cerr << "frame:" << frameCount << std::endl;
    MainRender();
}
