#pragma once
#include <StreamDeck/Layout.hpp>

#include <QPlainTextEdit>
#include <QWidget>

#include <verdigris>

class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QScrollArea;
class QStackedWidget;
namespace score
{
struct DocumentContext;
}
namespace Process
{
class AddressAccessorEditWidget;
}

namespace StreamDeck
{
class ActionList;
class ColorButton;
class FeedbackList;
class OverrideEditor;

//! A multi-line text field that reports an edit once, when it loses focus.
class TextField final : public QPlainTextEdit
{
  W_OBJECT(TextField)
public:
  using QPlainTextEdit::QPlainTextEdit;
  void editingFinished() W_SIGNAL(editingFinished);

protected:
  void focusOutEvent(QFocusEvent* e) override;
};

//! The inspector of the key or dial selected in the editor.
class ButtonEditor final : public QWidget
{
  W_OBJECT(ButtonEditor)
public:
  ButtonEditor(const score::DocumentContext& ctx, QWidget* parent = nullptr);

  //! defaultName: for the addresses its actions and feedbacks create.
  void showButton(
      const QString& title, const Button& b, QStringList pages, QString defaultName);
  void showDial(
      const QString& title, const Dial& d, QStringList pages, QString defaultName);
  void showMessage(const QString& text);
  void setScriptError(const QString& error);

  Button button() const;
  Dial dial() const;

  void buttonChanged() W_SIGNAL(buttonChanged);
  void dialChanged() W_SIGNAL(dialChanged);

private:
  QWidget* makeButtonForm();
  QWidget* makeDialForm();
  void updateModeLabels();

  const score::DocumentContext& m_ctx;
  QStackedWidget* m_stack{};
  QLabel* m_message{};

  QLabel* m_buttonTitle{};
  QLabel* m_buttonError{};
  QLabel* m_dialError{};
  TextField* m_text{};
  ColorButton* m_textColor{};
  ColorButton* m_background{};
  QSpinBox* m_textSize{};
  QComboBox* m_align{};
  QLineEdit* m_image{};
  QComboBox* m_mode{};
  QSpinBox* m_hold{};
  QLabel* m_activeLabel{};
  OverrideEditor* m_active{};
  QLabel* m_pressLabel{};
  ActionList* m_press{};
  QLabel* m_releaseLabel{};
  ActionList* m_release{};
  FeedbackList* m_feedbacks{};

  QLabel* m_dialTitle{};
  Process::AddressAccessorEditWidget* m_dialAddress{};
  QDoubleSpinBox* m_dialStep{};
  TextField* m_dialText{};
  ColorButton* m_dialTextColor{};
  ColorButton* m_dialBackground{};
  ActionList* m_dialPress{};
  QString m_dialAddressText;
};
}
