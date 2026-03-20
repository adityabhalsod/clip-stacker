#pragma once

#include <QDialog>

class QKeySequenceEdit;
class QSpinBox;
class SettingsManager;

// Present a compact native settings dialog for the small set of user-facing options.
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    // Construct the dialog around the shared runtime settings manager.
    explicit SettingsDialog(SettingsManager *settingsManager, QWidget *parent = nullptr);

private slots:
    // Persist the current form values back into the runtime settings store.
    void saveSettings();

protected:
    // Intercept Escape on the hotkey field to clear it instead of reverting the partial sequence.
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    // Load the current settings snapshot into the form controls.
    void loadSettings();

    // Keep the settings backend available while the dialog is open.
    SettingsManager *m_settingsManager = nullptr;

    // Edit the maximum number of retained clipboard items (range: 1–500).
    QSpinBox *m_historyLimitSpinBox = nullptr;

    // Capture the key combination that globally opens the clipboard popup.
    QKeySequenceEdit *m_hotkeyEdit = nullptr;
};
