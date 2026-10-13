#pragma once

#include <QWidget>
#include <QImage>

namespace Kaburagi {

class Document;

class PreviewWindow : public QWidget {
    Q_OBJECT
public:
    explicit PreviewWindow(QWidget* parent = nullptr);
    ~PreviewWindow() override;

    void setDocument(Document* doc);
    Document* document() const { return m_document; }

public slots:
    void refreshPreview();

signals:
    void windowClosed();

protected:
    void paintEvent(QPaintEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    Document* m_document = nullptr;
    QImage m_cachedComposite;
    bool m_dirty = true;
};

} // namespace Kaburagi
