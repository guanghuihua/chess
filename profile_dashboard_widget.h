#ifndef PROFILE_DASHBOARD_WIDGET_H
#define PROFILE_DASHBOARD_WIDGET_H

#include <QWidget>

#include "game_database.h"

class ProfileDashboardWidget : public QWidget
{
public:
    explicit ProfileDashboardWidget(QWidget *parent = nullptr);

    void setProfileData(const QString &userName,
                        const GameDatabase::UserProfile &profile,
                        const GameDatabase::TrainingStats &stats,
                        const GameDatabase::TrainingSummary &training,
                        const QVector<GameDatabase::GamePerformance> &recentGames);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString user_name_;
    GameDatabase::UserProfile profile_;
    GameDatabase::TrainingStats stats_;
    GameDatabase::TrainingSummary training_;
    QVector<GameDatabase::GamePerformance> recent_games_;

    static void drawMetricCard(class QPainter &painter, const QRectF &rect,
                               const QString &title, const QString &value,
                               const QColor &accent);
    void drawResultChart(class QPainter &painter, const QRectF &rect) const;
    void drawErrorChart(class QPainter &painter, const QRectF &rect) const;
    void drawTrendChart(class QPainter &painter, const QRectF &rect) const;
};

#endif // PROFILE_DASHBOARD_WIDGET_H
