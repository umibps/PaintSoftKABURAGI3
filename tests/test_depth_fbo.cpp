#include <QApplication>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <iostream>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QOpenGLContext ctx;
    QSurfaceFormat fmt;
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    ctx.setFormat(fmt);
    if (!ctx.create()) return 1;

    QOffscreenSurface surf;
    surf.setFormat(ctx.format());
    surf.create();
    ctx.makeCurrent(&surf);

    QOpenGLFramebufferObjectFormat fboFmt;
    fboFmt.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    QOpenGLFramebufferObject fbo(100, 100, fboFmt);

    auto gl = ctx.functions();
    GLint depthBits = 0;
    gl->glGetIntegerv(GL_DEPTH_BITS, &depthBits);
    std::cout << "GL_DEPTH_BITS: " << depthBits << std::endl;
    std::cout << "fbo.isValid: " << fbo.isValid() << " attachment: " << fbo.attachment() << std::endl;
    return 0;
}
