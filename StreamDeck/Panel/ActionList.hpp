#pragma once
#include <StreamDeck/Layout.hpp>

#include <QWidget>

#include <verdigris>

class QCheckBox;
class QLineEdit;
class QVBoxLayout;
namespace score
{
struct DocumentContext;
}

namespace StreamDeck
{
class ColorButton;

//! Edits a list of actions: what a key does when pressed or released.
class ActionList final : public QWidget
{
  W_OBJECT(ActionList)
public:
  ActionList(const score::DocumentContext& ctx, QWidget* parent = nullptr);

  void setActions(std::vector<Action> actions, QStringList pages);
  const std::vector<Action>& actions() const noexcept { return m_actions; }

  void changed() W_SIGNAL(changed);

private:
  void rebuild();
  void commit();
  QWidget* makeRow(int i);

  const score::DocumentContext& m_ctx;
  std::vector<Action> m_actions;
  QStringList m_pages;
  QVBoxLayout* m_rows{};
};

//! Edits the feedbacks of a key: how its look follows a value of the document.
class FeedbackList final : public QWidget
{
  W_OBJECT(FeedbackList)
public:
  FeedbackList(const score::DocumentContext& ctx, QWidget* parent = nullptr);

  void setFeedbacks(std::vector<Feedback> f);
  const std::vector<Feedback>& feedbacks() const noexcept { return m_feedbacks; }

  void changed() W_SIGNAL(changed);

private:
  void rebuild();
  void commit();
  QWidget* makeRow(int i);

  const score::DocumentContext& m_ctx;
  std::vector<Feedback> m_feedbacks;
  QVBoxLayout* m_rows{};
};

//! Edits the fields of a style that a state replaces.
class OverrideEditor final : public QWidget
{
  W_OBJECT(OverrideEditor)
public:
  explicit OverrideEditor(QWidget* parent = nullptr);

  void setOverride(const StyleOverride& o);
  StyleOverride value() const;

  void changed() W_SIGNAL(changed);

private:
  QCheckBox* m_useBackground{};
  ColorButton* m_background{};
  QCheckBox* m_useTextColor{};
  ColorButton* m_textColor{};
  QCheckBox* m_useText{};
  QLineEdit* m_text{};
};
}
