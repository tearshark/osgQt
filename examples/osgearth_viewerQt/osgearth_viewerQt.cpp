#define GLINTPTR_DEFINED__

#include <QApplication>
#include <QGridLayout>
#include <osgQOpenGL/osgQOpenGLWidget>

#include <osgEarth/MapNode>
#include <osgEarth/URI>
#include <osgEarth/EarthManipulator>
#include <osgEarth/ExampleResources>
#include <osgEarth/LogarithmicDepthBuffer>
#include <osgEarth/GLUtils>

#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>
#include <osgDB/ReadFile>

#include <osgGA/TrackballManipulator>
#include <osgGA/FlightManipulator>
#include <osgGA/DriveManipulator>
#include <osgGA/KeySwitchMatrixManipulator>
#include <osgGA/StateSetManipulator>
#include <osgGA/AnimationPathManipulator>
#include <osgGA/TerrainManipulator>
#include <osgGA/SphericalManipulator>

#include <osgGA/Device>

#ifdef _WIN32
#define EARTH_FILE "E:/AtomSpace/xwd/xwd/GisData/Earth.earth"
#define MOON_IMAGE_FILE "E:/AtomData/AtomSpace/xwd/xwd/resources/Images/moon_1024x512.jpg"
#else
#define EARTH_FILE "/home/shark/dev/gisdata/Earth.earth"
#define MOON_IMAGE_FILE "/home/shark/dev/gisdata/Images/moon_1024x512.jpg"
#endif

#define LC "test_EarthSingleView"

namespace
{
    struct MultiRealizeOperation : public osg::Operation
    {
        void operator()(osg::Object* obj) override
        {
            for (auto& op : _ops)
                op->operator()(obj);
        }
        std::vector<osg::ref_ptr<osg::Operation>> _ops;
    };

    // 只能在视图 realize 以后执行的操作。不可异步执行
    void realizeViewer(osgViewer::ViewerBase* viewer)
    {
        if (viewer->isRealized())
        {
            osgEarth::GL3RealizeOperation rop;
            rop.setSyncToVBlank(false);

            osgViewer::Viewer::Views views;
            viewer->getViews(views);

            for (osgViewer::View* view : views)
            {
                osg::Camera* camera = view->getCamera();
                if (camera != nullptr)
                {
                    rop(camera->getGraphicsContext());
                }
            }
        }
        else
        {
            MultiRealizeOperation* op = new MultiRealizeOperation();

            if (viewer->getRealizeOperation())
                op->_ops.push_back(viewer->getRealizeOperation());

            osgEarth::GL3RealizeOperation* rop = new osgEarth::GL3RealizeOperation();
            rop->setSyncToVBlank(false);

            op->_ops.push_back(rop);

            viewer->setRealizeOperation(op);
        }
    }

    // 随时可以执行的操作。不可异步执行
    void connectExtensions(osgViewer::View* view, osgEarth::MapNode* mapNode)
    {
        assert(view != nullptr);
        assert(mapNode != nullptr);

        for (auto& extension : mapNode->getExtensions())
        {
            // Check for a View interface:
            osgEarth::ExtensionInterface<osg::View>* viewIF = osgEarth::ExtensionInterface<osg::View>::get(extension.get());
            if (viewIF)
                viewIF->connect(view);
        }
    }

    // 随时可以执行的操作。不可异步执行
    void configureView(osgViewer::View* view)
    {
        assert(view != nullptr);

        osg::Camera* camera = view->getCamera();
        if (camera != nullptr)
        {
            // default uniform values:
            osgEarth::GLUtils::setGlobalDefaults(camera->getOrCreateStateSet());

            // disable small feature culling (otherwise Text annotations won't render)
            camera->setSmallFeatureCullingPixelSize(-1.0f);

            // Install logarithmic depth buffer on main camera
            if (false)
            {
                OE_INFO << LC << "Activating logarithmic depth buffer (vertex-only) on main camera" << std::endl;
                osgEarth::Util::LogarithmicDepthBuffer logDepth;
                logDepth.setUseFragDepth(false);
                logDepth.install(camera);
            }
        }
    }

    // 创建天空扩展。可异步执行
    osg::ref_ptr<osgEarth::SkyNode> createSkyExtension(osgEarth::MapNode* mapNode, const osgEarth::Config& conf)
    {
        assert(mapNode != nullptr);

        std::string skyType = mapNode->getMapSRS()->isGeographic() ? "sky_simple" : "sky_gl";
        osgEarth::SkyOptions options(conf);
        options.quality() = osgEarth::SkyOptions::QUALITY_DEFAULT;
        //options.ambient() = 0.1f;
        osg::ref_ptr<osgEarth::Extension> ext = osgEarth::Extension::create(skyType, options);
        if (ext == nullptr)
        {
            return nullptr;
        }
        mapNode->addExtension(ext);

        osg::ref_ptr<osgEarth::SkyNode> skyNode = dynamic_cast<osgEarth::SkyNode*>(mapNode->getParent(0));
        if (skyNode != nullptr && skyNode->getSunLight() != nullptr)
        {
            skyNode->setLighting(osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE | osg::StateAttribute::PROTECTED);

            osg::Light* light = skyNode->getSunLight();
            light->setAmbient(osg::Vec4{ 0.1f, 0.1f, 0.1f, 1.0f });
            light->setDiffuse(osg::Vec4{ 0.0f, 0.1f, 0.0f, 1.0f });
            light->setSpecular(osg::Vec4{ 0.0f, 0.0f, 0.1f, 1.0f });
        }

        return skyNode;
    }

    std::pair<osg::ref_ptr<osg::Group>, osg::ref_ptr<osgEarth::MapNode>>
        createMapNode(const char* filePath)
    {
#if 0
        osgEarth::URI uri(filePath);
        osgEarth::ReadResult r = uri.readNode();
        if (!r.succeeded())
        {
            return { nullptr, nullptr };
        }

        osg::ref_ptr<osgEarth::MapNode> mapNode = r.release<osgEarth::MapNode>();
#else
        char* argv[] = { nullptr, const_cast<char*>(filePath), nullptr };
        int argc = sizeof(argv) / sizeof(argv[0]) - 1;
        osg::ArgumentParser args(&argc, argv);

        // read in the Earth file:
        osg::ref_ptr<osg::Node> node = osgDB::readNodeFiles(args);

        // fallback in case none is specified:
        if (!node.valid())
        {
            OE_WARN << LC << "No valid earth file loaded - aborting" << std::endl;
            return { nullptr, nullptr };
        }

        osg::ref_ptr<osgEarth::MapNode> mapNode = osgEarth::MapNode::get(node.get());
        if (!mapNode.valid())
        {
            OE_WARN << LC << "Loaded scene graph does not contain a MapNode - aborting" << std::endl;
            return { nullptr, nullptr };
        }
#endif
        // a root node to hold everything:
        osg::ref_ptr<osg::Group> root = new osg::Group();
        root->addChild(mapNode);

        // open the map node:
        if (!mapNode->open())
        {
            OE_WARN << LC << "Failed to open MapNode" << std::endl;
            return { nullptr, nullptr };
        }

        return { root, mapNode };
    }
}

int main(int argc, char** argv)
{
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();

#ifdef OSG_GL3_AVAILABLE
    format.setVersion(3, 2);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setOption(QSurfaceFormat::DebugContext);
#else
    format.setVersion(2, 0);
    format.setProfile(QSurfaceFormat::CompatibilityProfile);
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setOption(QSurfaceFormat::DebugContext);
#endif
    format.setDepthBufferSize(24);
    //format.setAlphaBufferSize(8);
    format.setSamples(8);
    format.setStencilBufferSize(8);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts, true);

    QApplication app(argc, argv);

    osgEarth::initialize();

    // thread-safe initialization of the OSG wrapper manager. Calling this here
    // prevents the "unsupported wrapper" messages from OSG
    osgDB::Registry::instance()->getObjectWrapperManager()->findWrapper("osg::Image");
    auto& filepaths = osgDB::Registry::instance()->getDataFilePathList();
    filepaths.push_back((QApplication::applicationDirPath() + "/shaders").toStdString());

    osgQOpenGLWidget widget;

    QObject::connect(&widget, &osgQOpenGLWidget::initialized, [&widget]
        {
            // set up the camera manipulators.
            widget.getOsgViewer()->setCameraManipulator(new osgEarth::Util::EarthManipulator());

            // add the state manipulator
            widget.getOsgViewer()->addEventHandler(new osgGA::StateSetManipulator(widget.getOsgViewer()->getCamera()->getOrCreateStateSet()));

            // add the thread model handler
            widget.getOsgViewer()->addEventHandler(new osgViewer::ThreadingHandler);

            // add the window size toggle handler
            widget.getOsgViewer()->addEventHandler(new osgViewer::WindowSizeHandler);

            // add the stats handler
            widget.getOsgViewer()->addEventHandler(new osgViewer::StatsHandler);

            // add the record camera path handler
            widget.getOsgViewer()->addEventHandler(new osgViewer::RecordCameraPathHandler);

            // add the LOD Scale handler
            widget.getOsgViewer()->addEventHandler(new osgViewer::LODScaleHandler);

            // add the screen capture handler
            widget.getOsgViewer()->addEventHandler(new osgViewer::ScreenCaptureHandler);

#if 0
            {
                char filePath[] = EARTH_FILE;
                char skyOpt[] = "--sky";
                char* argv[] = { nullptr, filePath, skyOpt, nullptr };
                int argc = sizeof(argv) / sizeof(argv[0]) - 1;
                osg::ArgumentParser arguments(&argc, argv);

                auto* viewer = widget.getOsgViewer();
                osg::Group* node = osgEarth::Util::MapNodeHelper().loadWithoutControls(arguments, viewer);
                if (node != nullptr)
                {
                    node->removeChildren(1, node->getNumChildren());
                    widget.getOsgViewer()->setSceneData(node);
                }
                else
                {
                    widget.getOsgViewer()->setSceneData(osgDB::readRefNodeFile("axes.osgt"));
                }
            }
#else
            osg::ref_ptr<osg::Group> root;
            osg::ref_ptr<osgEarth::MapNode> mapNode;
            std::tie(root, mapNode) = createMapNode(EARTH_FILE);
            if (mapNode)
            {
                assert(root != nullptr);

                osgEarth::Config conf;
                //conf.set("atmospheric_lighting", false);
                conf.set("exposure", 2.0);
                conf.set("daytime_ambient_boost", 2.0f);
                //conf.set("star_file", _starFile);
                //conf.set("star_size", _starSize);
                //conf.set("allow_wireframe", _allowWireframe);
                conf.set("sun_visible", true);
                conf.set("moon_visible", true);
                conf.set("stars_visible", true);
                conf.set("atmosphere_visible", true);
                conf.set("moon_scale", 1.0);
                conf.set("moon_image", MOON_IMAGE_FILE);
                //conf.set("pbr", _pbr);
                //conf.set("eb", _eb);

                auto sky = createSkyExtension(mapNode, conf);
                configureView(widget.getOsgViewer());
                connectExtensions(widget.getOsgViewer(), mapNode);
                realizeViewer(widget.getOsgViewer());

                widget.getOsgViewer()->setSceneData(root);
            }
            else
            {
                widget.getOsgViewer()->setSceneData(osgDB::readRefNodeFile("axes.osgt"));
            }
#endif

            return 0;
        });


    widget.show();

    return app.exec();
}

#if 0

第一步：加载地球，可异步执行

// read in the Earth file:
osg::ref_ptr<osg::Node> node = osgDB::readNodeFiles(args, myReadOptions.get());

// fallback in case none is specified:
if (!node.valid())
{
    OE_WARN << LC << "No valid earth file loaded - aborting" << std::endl;
    return nullptr;
}

osg::ref_ptr<MapNode> mapNode = MapNode::get(node.get());
if (!mapNode.valid())
{
    OE_WARN << LC << "Loaded scene graph does not contain a MapNode - aborting" << std::endl;
    return nullptr;
}

// a root node to hold everything:
osg::ref_ptr<osg::Group> root = new osg::Group();
root->addChild(node);

// open the map node:
if (!mapNode->open())
{
    OE_WARN << LC << "Failed to open MapNode" << std::endl;
    return nullptr;
}

// Install logarithmic depth buffer on main camera
if (useLogDepth)
{
    OE_INFO << LC << "Activating logarithmic depth buffer (vertex-only) on main camera" << std::endl;
    osgEarth::Util::LogarithmicDepthBuffer logDepth;
    logDepth.setUseFragDepth(false);
    logDepth.install(view->getCamera());
}

第二步：创建天空，可异步
if (sky_quality != SkyOptions::QUALITY_UNSET && mapNode->open())
{
    std::string ext = mapNode->getMapSRS()->isGeographic() ? "sky_simple" : "sky_gl";
    SkyOptions options;
    options.quality() = sky_quality;
    mapNode->addExtension(Extension::create(ext, options));
}

第三步：与视图绑定，不可异步
    1、扩展链接子视图
if (mapNode)
{
    for (auto& extension : mapNode->getExtensions())
    {
        // Check for a View interface:
        ExtensionInterface<osg::View>* viewIF = ExtensionInterface<osg::View>::get(extension.get());
        if (viewIF)
            viewIF->connect(view);
    }
}

    2、配置子视图
// configures each view with some stock goodies
for (osgViewer::Viewer::Views::iterator view = views.begin(); view != views.end(); ++view)
{
    configureView(*view);
}

    3、初始化组合视图
if (viewer)
{
    if (viewer->isRealized())
    {
        osgEarth::GL3RealizeOperation rop;

        // configures each view with some stock goodies
        for (osgViewer::Viewer::Views::iterator iter = views.begin(); iter != views.end(); ++iter)
        {
            osgViewer::View* view = *iter;

            osg::Camera* camera = view->getCamera();
            if (camera != nullptr)
            {
                rop(camera->getGraphicsContext());
            }
        }
    }
    else
    {
        MultiRealizeOperation* op = new MultiRealizeOperation();

        if (viewer->getRealizeOperation())
            op->_ops.push_back(viewer->getRealizeOperation());

        GL3RealizeOperation* rop = new GL3RealizeOperation();
        if (vsync.isSet())
            rop->setSyncToVBlank(vsync.get());

        op->_ops.push_back(rop);

        viewer->setRealizeOperation(op);
    }
}


void
MapNodeHelper::configureView(osgViewer::View * view) const
{
    // default uniform values:
    GLUtils::setGlobalDefaults(view->getCamera()->getOrCreateStateSet());

    // disable small feature culling (otherwise Text annotations won't render)
    view->getCamera()->setSmallFeatureCullingPixelSize(-1.0f);

    // thread-safe initialization of the OSG wrapper manager. Calling this here
    // prevents the "unsupported wrapper" messages from OSG
    osgDB::Registry::instance()->getObjectWrapperManager()->findWrapper("osg::Image");

    // add some stock OSG handlers:
    view->addEventHandler(new osgViewer::StatsHandler());
    view->addEventHandler(new osgViewer::WindowSizeHandler());
    view->addEventHandler(new osgViewer::ThreadingHandler());
    view->addEventHandler(new osgViewer::LODScaleHandler());
    view->addEventHandler(new osgGA::StateSetManipulator(view->getCamera()->getOrCreateStateSet()));
    view->addEventHandler(new osgViewer::RecordCameraPathHandler());
    view->addEventHandler(new osgViewer::ScreenCaptureHandler());
}

--sky
moon_1024x512.jpg
if (args.read("--shadows") && mapNode)

#endif
