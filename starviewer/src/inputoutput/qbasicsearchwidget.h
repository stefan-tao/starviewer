
#ifndef UDGQBASICSEARCHWIDGET_H
#define UDGQBASICSEARCHWIDGET_H

#include <ui_qbasicsearchwidgetbase.h>

class QString;

namespace udg {

class DicomMask;

/** Classe de cerca bàsica d'elemens
	@author Grup de Gràfics de Girona  ( GGG ) <vismed@ima.udg.es>
*/
class QBasicSearchWidget : public QWidget , private Ui::QBasicSearchWidgetBase{
Q_OBJECT
public:
    enum DefaultDateFlags{ AnyDate, Today, Yesterday, LastWeek };
    QBasicSearchWidget( QWidget *parent = 0 );
    ~QBasicSearchWidget();

    ///neteja els camps de la cerca bàsica
    void clear();

    ///Activa o desactiva poder escollir la modalitat de serie al fer la cerca
    void setEnabledSeriesModality(bool enabled);

    ///Construeix la màscara de cerca
    DicomMask buildDicomMask();

    /// Ens marca les flags de cerca per data per defecte
    void setDefaultDate( DefaultDateFlags flag );

private slots :
    /** Slot que s'activa quan s'ha editat el m_textOtherModality, en cas que el text sigui <> "" deselecciona totes les modalitats,
     * i en cas que sigui = "" selecciona la modalitat checkAll
    */  
    void textOtherModalityEdited();

    /**
     * Checkeig de les dates que canvien els QDateEdit per mantenir consistència
     * @param date
     */
    void checkFromDate(QDate date);
    void checkToDate(QDate date);

private:
    ///Crea les connexions entre signals i slots
    void createConnections();

    ///Inicialitza els valors de la QBasicSearchWidget
    void initialize();

    /// construeix la màscara de les dates
    QString getStudyDatesStringMask();

    ///Neteja el paràmetre de cerca de modalitat de serie
    void clearSeriesModality();

    ///S'executa quan es mostra el widget, fa que la primera vegada que es mostri es doni el focus al lineEdit PatientName
    void showEvent(QShowEvent * event);

private:

    ///Indica si s'ha mostra el widget
    bool m_widgetHasBeenShowed;
};

}

#endif
