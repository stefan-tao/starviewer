/***************************************************************************
 *   Copyright (C) 2005-2006 by Grup de Gràfics de Girona                  *
 *   http://iiia.udg.es/GGG/index.html?langu=uk                            *
 *                                                                         *
 *   Universitat de Girona                                                 *
 ***************************************************************************/
#include "starviewerprocessimagestored.h"
#include <iostream.h>

namespace udg {

StarviewerProcessImageStored::StarviewerProcessImageStored(QObject *parent)
 : QObject(parent)
{
    m_imagesStored = 0;
}

void StarviewerProcessImageStored::process( Image *image )
{
    if ( m_imagesStored == 0 ) // és la primera imatge que guardem
    {
      m_oldSeriesUID = image->getSeriesUID().c_str();
      m_studyUID = image->getStudyUID().c_str();
    }

    if ( m_oldSeriesUID != image->getSeriesUID().c_str() ) // canviem de serie d'imatges guardades
    {
        emit( seriesStored( image->getStudyUID().c_str() ) );
        m_oldSeriesUID = image->getSeriesUID().c_str();
    }

    m_imagesStored++;
    emit( imageStored( image->getStudyUID().c_str() , m_imagesStored ) );
}

StarviewerProcessImageStored::~StarviewerProcessImageStored()
{
    //com no sabem quant s'acaba de guardar l'última sèrie, fem que s'indiqui que ha finalitzat el procés de moure quan es destrueix l'objecte StarviewerProcessImageStored, que és destruït just finalitzar el procés d'emmagatzematge de tot l'estudi
    emit( seriesStored( m_studyUID ) );
}


}
