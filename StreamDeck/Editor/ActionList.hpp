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

class ActionList final : public QWidget
{
  W_OBJECT(ActionList)
public:
  ActionList(const score::DocumentContext& ctx, QWidget* parent = nullptr);

  void setActions(std::vector<Action> actions, QStringList pages, QString defaultName);
  const std::vector<Action>& actions() const noexcept { return m_actions; }

  void changed() W_SIGNAL(changed);

private:
  void rebuild();
  void commit();
  QWidget* makeRow(int i);

  const score::DocumentContext& m_ctx;
  std::vector<Action> m_actions;
  QStringList m_pages;
  QString m_defaultName;
  QVBoxLayout* m_rows{};
};

class FeedbackList final : public QWidget
{
  W_OBJECT(FeedbackList)
public:
  FeedbackList(const score::DocumentContext& ctx, QWidget* parent = nullptr);

  void setFeedbacks(std::vector<Feedback> f, QString defaultName);
  const std::vector<Feedback>& feedbacks() const noexcept { return m_feedbacks; }

  void changed() W_SIGNAL(changed);

private:
  void rebuild();
  void commit();
  QWidget* makeRow(int i);

  const score::DocumentContext& m_ctx;
  std::vector<Feedback> m_feedbacks;
  QString m_defaultName;
  QVBoxLayout* m_rows{};
};

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
