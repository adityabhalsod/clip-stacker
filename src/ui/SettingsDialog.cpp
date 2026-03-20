#include "SettingsDialog.h"

#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QKeyEvent>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include "core/SettingsManager.h"

SettingsDialog::SettingsDialog(SettingsManager *settingsManager, QWidget *parent)
    : QDialog(parent)
    , m_settingsManager(settingsManager)
{
    // Give the dialog a simple native title with enough height for both form rows.
    setWindowTitle(QStringLiteral("clip-stacker Settings"));
    resize(440, 230);

    auto *mainLayout = new QVBoxLayout(this);
    auto *descriptionLabel = new QLabel(QStringLiteral("Configure clipboard retention and global hotkey."), this);
    auto *formLayout = new QFormLayout();

    // Constrain the history limit to 1–500: rejects negatives, zero, and non-numeric input naturally.
    m_historyLimitSpinBox = new QSpinBox(this);
    m_historyLimitSpinBox->setRange(1, 500);          // Max 500 prevents excessive memory use
    m_historyLimitSpinBox->setSuffix(QStringLiteral(" items"));
    m_historyLimitSpinBox->setToolTip(QStringLiteral("Number of clipboard entries to keep (1–500)."));
    formLayout->addRow(QStringLiteral("History limit"), m_historyLimitSpinBox);

    // Let the user record a custom key combination for the global popup shortcut.
    m_hotkeyEdit = new QKeySequenceEdit(this);
    m_hotkeyEdit->setToolTip(QStringLiteral("Press a single key combination to set the popup hotkey.\nMeta = Win / Super key.\nPress Esc to clear."));
    // Intercept Escape before QKeySequenceEdit's own keyPressEvent can revert the sequence.
    m_hotkeyEdit->installEventFilter(this);
    // Enforce exactly one key chord.
    // Strategy: each time keySequenceChanged fires, trim to the first chord;
    // then if the chord is complete (has a non-modifier main key), immediately
    // remove focus so QKeySequenceEdit stops its internal "waiting for more" timer.
    connect(m_hotkeyEdit, &QKeySequenceEdit::keySequenceChanged, this, [this](const QKeySequence &seq) {
        if (seq.isEmpty()) return;

        // Trim any extra chords down to just the first one.
        if (seq.count() > 1) {
            const QSignalBlocker blocker(m_hotkeyEdit);
            m_hotkeyEdit->setKeySequence(QKeySequence(seq[0]));
        }

        // Detect whether the first chord is complete (i.e. has a real key, not only modifiers).
        const Qt::Key mainKey = seq[0].key();
        const bool isModifierOnly = (mainKey == Qt::Key_Shift   ||
                                     mainKey == Qt::Key_Control ||
                                     mainKey == Qt::Key_Alt     ||
                                     mainKey == Qt::Key_Meta    ||
                                     mainKey == Qt::Key_unknown ||
                                     mainKey == Qt::Key_AltGr   ||
                                     mainKey == Qt::Key_Super_L ||
                                     mainKey == Qt::Key_Super_R ||
                                     mainKey == Qt::Key_Hyper_L ||
                                     mainKey == Qt::Key_Hyper_R);
        if (!isModifierOnly) {
            // Defer clearFocus one event-loop tick so Qt can finish processing the key release,
            // then steal focus to stop the widget's internal "waiting for next chord" timer.
            QTimer::singleShot(0, this, [this]() { m_hotkeyEdit->clearFocus(); });
        }
    });
    formLayout->addRow(QStringLiteral("Popup hotkey"), m_hotkeyEdit);

    // Remind users that Meta = Win/Super and that Escape clears the recorded hotkey.
    auto *hotkeyHint = new QLabel(
        QStringLiteral("<small><i>Meta\u00a0=\u00a0Win\u00a0/\u00a0Super\u00a0key\u2002\u2002"
                       "Default:\u00a0Ctrl+Meta+V\u2002\u2002Esc\u00a0=\u00a0clear</i></small>"),
        this);

    // Assemble the layout: description, form rows, hint, then action buttons.
    mainLayout->addWidget(descriptionLabel);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(hotkeyHint);
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttonBox);

    // Connect the standard dialog buttons.
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::saveSettings);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);

    // Populate the form from the current persistent settings.
    loadSettings();
}

void SettingsDialog::saveSettings()
{
    SettingsManager::SettingsSnapshot snapshot = m_settingsManager->snapshot();

    // Clamp to the declared range before persisting — guards against programmatic out-of-range values.
    snapshot.historyLimit = qBound(1, m_historyLimitSpinBox->value(), 500);

    // Extract only the first key chord so the stored sequence is always a single accelerator.
    const QKeySequence seq = m_hotkeyEdit->keySequence();
    if (!seq.isEmpty()) {
        // Portable text uses cross-platform modifier names (e.g. "Ctrl+Meta+V").
        snapshot.hotkey = QKeySequence(seq[0]).toString(QKeySequence::PortableText);
    }

    m_settingsManager->setSnapshot(snapshot);
    accept();
}

bool SettingsDialog::eventFilter(QObject *watched, QEvent *event)
{
    // Only intercept key presses directed at the hotkey capture widget.
    if (watched == m_hotkeyEdit && event->type() == QEvent::KeyPress) {
        const auto *keyEvent = static_cast<QKeyEvent *>(event);
        // Clear the recorded sequence on Escape so the user can easily start fresh.
        if (keyEvent->key() == Qt::Key_Escape) {
            m_hotkeyEdit->clear();
            return true; // Consume the event — prevent QKeySequenceEdit from reverting
        }
    }
    // Let all other events pass through to the target widget unchanged.
    return QDialog::eventFilter(watched, event);
}

void SettingsDialog::loadSettings()
{
    const SettingsManager::SettingsSnapshot snapshot = m_settingsManager->snapshot();

    // Reflect the persisted limit, clamped so out-of-range legacy values still render correctly.
    m_historyLimitSpinBox->setValue(qBound(1, snapshot.historyLimit, 500));

    // Normalize legacy "Super"/"Win" tokens to the Qt "Meta" name so QKeySequenceEdit can display them.
    QString hotkeyStr = snapshot.hotkey;
    hotkeyStr.replace(QStringLiteral("Super"), QStringLiteral("Meta"), Qt::CaseInsensitive);
    hotkeyStr.replace(QStringLiteral("Win"),   QStringLiteral("Meta"), Qt::CaseInsensitive);
    m_hotkeyEdit->setKeySequence(QKeySequence(hotkeyStr, QKeySequence::PortableText));
}
