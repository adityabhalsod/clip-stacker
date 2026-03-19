#pragma once

#include <QObject>
#include <QTimer>

#include "ClipboardEntry.h"

class QClipboard;

// Observe live clipboard changes and convert raw mime data into normalized history entries.
class ClipboardListener : public QObject
{
    Q_OBJECT

public:
    // Construct the listener around the process clipboard and a debounce timer.
    explicit ClipboardListener(QObject *parent = nullptr);

    // Start monitoring clipboard updates from the current desktop session.
    void start();

    // Suppress the next clipboard change when the application restores an item for pasting.
    void suppressNextChange();

signals:
    // Emit a fully normalized clipboard entry that the history manager can persist and display.
    void entryCaptured(const ClipboardEntry &entry);

private slots:
    // Convert the current clipboard payload into a normalized entry after the debounce interval expires.
    void processClipboardChange();

private:
    // Detect the source application when the platform exposes an active-window class.
    QString currentSourceApplication() const;

    // Build a stable hash used to deduplicate equivalent clipboard payloads across sessions.
    QString contentHashForEntry(const ClipboardEntry &entry) const;

    // Convert raw text into a compact single-line preview for the popup list.
    QString compactPreview(const QString &text) const;

    // Access the live system clipboard owned by the current GUI session.
    QClipboard *m_clipboard = nullptr;

    // Debounce noisy clipboard updates triggered by some applications during copy operations.
    QTimer m_debounceTimer;

    // Track self-originated clipboard writes so activation does not duplicate history entries.
    bool m_suppressNextChange = false;
};
