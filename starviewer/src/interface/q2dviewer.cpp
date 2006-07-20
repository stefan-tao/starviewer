/***************************************************************************
 *   Copyright (C) 2005 by Grup de Gr�fics de Girona                       *
 *   http://iiia.udg.es/GGG/index.html?langu=uk                            *
 *                                                                         *
 *   Universitat de Girona                                                 *
 ***************************************************************************/

#include "q2dviewer.h"
#include "volume.h"
#include "volumesourceinformation.h"
#include "logging.h"

// include's qt
#include <QResizeEvent>
#include <QSize>
#include <QMenu>
#include <QAction>

// Tools

// include's vtk
#include <QVTKWidget.h>
#include <vtkEventQtSlotConnect.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkImageViewer2.h>
// composici� d'imatges
#include <vtkImageCheckerboard.h>
#include <vtkImageBlend.h> 
#include <vtkImageRectilinearWipe.h>
#include <vtkImageMapToWindowLevelColors.h>
#include <vtkPropPicker.h>
#include <vtkCornerAnnotation.h>
#include <vtkTextProperty.h>
#include <vtkAxisActor2D.h>
#include <vtkProperty2D.h>
#include <vtkTextActor.h>
#include <vtkWindowToImageFilter.h>
#include <vtkPNGWriter.h>
#include <vtkPNMWriter.h>
#include <vtkJPEGWriter.h>
#include <vtkTIFFWriter.h>
#include <vtkBMPWriter.h>
#include <vtkCamera.h>
// voxel information
#include <vtkPointData.h>
#include <vtkCell.h>
#include <vtkCaptionActor2D.h>
// interacci�
#include <vtkInteractorStyleImage.h>

namespace udg {

class WindowLevelCallback : public vtkCommand
{
public:
    Q2DViewer* m_viewer;
    static WindowLevelCallback *New()
    {
       return new WindowLevelCallback;
    }
    virtual void Execute( vtkObject *caller, unsigned long event, void* )
    {
        vtkInteractorStyleImage *interactor = vtkInteractorStyleImage::SafeDownCast( caller );
        switch( event )
        {
        case vtkCommand::StartWindowLevelEvent:
        break;

        case vtkCommand::WindowLevelEvent:
            if( m_viewer->isManipulateOn() )
            {
                interactor->EndWindowLevel();
            }
            else
            {
                m_viewer->updateWindowLevelAnnotation();
            }
        break;

        case vtkCommand::EndWindowLevelEvent:
        break;
        }
    }
};

Q2DViewer::Q2DViewer( QWidget *parent , unsigned int annotations )
 : QViewer( parent )
{
    m_enabledAnnotations = annotations;
    m_lastView = None; 
    m_viewer = vtkImageViewer2::New();
    
    m_currentSlice = 0;
    m_overlay = CheckerBoard; // per defecte
    m_overlayVolume = 0;

// par�metres espec�fics
    
    // CheckerBoard
    // el nombre de divisions per defecte, ser� de 2, per simplificar
    m_divisions[0] = m_divisions[1] = m_divisions[2] = 2;
    
    // preparem el picker
    m_picker = vtkPropPicker::New();
    // ANOTACIONS
    createAnnotations();
    
    m_currentTool = Manipulate;
    
    m_leftButtonAction = Q2DViewer::CursorAction;
    m_middleButtonAction = Q2DViewer::SliceMotionAction;
    m_rightButtonAction = Q2DViewer::WindowLevelAction;
    
    createActions();    
    createTools();
    
    updateCursor( -1, -1, -1, -1 );
    m_voxelInformationCaption = vtkCaptionActor2D::New();
    m_voxelInformationCaption->GetAttachmentPointCoordinate()->SetCoordinateSystemToWorld();
    m_voxelInformationCaption->SetAttachmentPoint( m_currentCursorPosition );
    m_voxelInformationCaption->GetPositionCoordinate()->SetCoordinateSystemToWorld();
    m_voxelInformationCaption->BorderOff();
    m_voxelInformationCaption->LeaderOn();
    m_voxelInformationCaption->ThreeDimensionalLeaderOn();
    m_voxelInformationCaption->SetPadding( 1 );
    m_voxelInformationCaption->SetPosition( -1.0 , -1.0 );
    m_voxelInformationCaption->SetHeight( 0.05 );
    m_voxelInformationCaption->SetWidth( 0.3 );
    
    m_voxelInformationCaption->GetCaptionTextProperty()->SetColor( 1. , 0.7 , 0.0 );
    m_voxelInformationCaption->GetCaptionTextProperty()->ShadowOn();
    m_voxelInformationCaption->GetCaptionTextProperty()->ItalicOff();
    m_voxelInformationCaption->GetCaptionTextProperty()->BoldOff();
    this->getRenderer()->AddActor( m_voxelInformationCaption );
    
    m_windowToImageFilter->SetInput( this->getRenderer()->GetRenderWindow() );

    m_manipulateState = Q2DViewer::Ready;
    m_manipulating = false;
}

Q2DViewer::~Q2DViewer()
{
}

vtkRenderer *Q2DViewer::getRenderer()
{
    if( m_viewer )
        return m_viewer->GetRenderer();
    else
        return NULL;
}

vtkRenderWindowInteractor *Q2DViewer::getInteractor()
{
    return m_vtkWidget->GetRenderWindow()->GetInteractor();
}

void Q2DViewer::createActions()
{
    m_resetAction = new QAction( this );
    m_resetAction->setText(tr("&Reset"));
    m_resetAction->setShortcut( tr("Ctrl+R") );
    m_resetAction->setStatusTip(tr("Reset initial parameters"));
    connect( m_resetAction, SIGNAL( triggered() ), this, SLOT( reset() ) );
}

void Q2DViewer::createTools()
{
}

void Q2DViewer::createAnnotations()
{
    // anotacions textuals
    m_textAnnotation = vtkCornerAnnotation::New();
    // informaci� de refer�ncia de la orientaci� del pacient
    for( int i = 0; i < 4; i++ )
    {
        m_patientOrientationTextActor[i] = vtkTextActor::New();
        m_patientOrientationTextActor[i]->ScaledTextOff();
        m_patientOrientationTextActor[i]->GetTextProperty()->SetFontSize( 12 );
        m_patientOrientationTextActor[i]->GetTextProperty()->BoldOn();

        m_patientOrientationTextActor[i]->GetPositionCoordinate()->SetCoordinateSystemToNormalizedViewport();
        m_patientOrientationTextActor[i]->GetPosition2Coordinate()->SetCoordinateSystemToNormalizedViewport();

        this->getRenderer()->AddActor2D( m_patientOrientationTextActor[i] );
    }
    // ara posem la informaci� concreta de cadascuna de les refer�ncia d'orientaci�. 0-4 en sentit anti-horari, comen�ant per 0 = esquerra de la pantalla
    m_patientOrientationTextActor[0]->GetTextProperty()->SetJustificationToLeft();
    m_patientOrientationTextActor[0]->SetPosition( 0.01 , 0.5 );

    m_patientOrientationTextActor[1]->GetTextProperty()->SetJustificationToCentered();
    m_patientOrientationTextActor[1]->SetPosition( 0.5 , 0.01 );
    
    m_patientOrientationTextActor[2]->GetTextProperty()->SetJustificationToRight();
    m_patientOrientationTextActor[2]->SetPosition( 0.99 , 0.5 );

    m_patientOrientationTextActor[3]->GetTextProperty()->SetJustificationToCentered();
    m_patientOrientationTextActor[3]->SetPosition( 0.5 , 0.95 );

    // Marcadors
    m_sideRuler = vtkAxisActor2D::New();
    m_sideRuler->AxisVisibilityOn();
    m_sideRuler->TickVisibilityOn();
    m_sideRuler->LabelVisibilityOff();
    m_sideRuler->TitleVisibilityOff();
    m_sideRuler->SetTickLength( 10 );
    m_sideRuler->GetProperty()->SetColor( 0 , 1 , 0 );
    this->getRenderer()->AddActor2D( m_sideRuler );

    m_bottomRuler = vtkAxisActor2D::New();
    m_bottomRuler->AxisVisibilityOn();
    m_bottomRuler->TickVisibilityOn();
    m_bottomRuler->LabelVisibilityOff();
    m_bottomRuler->TitleVisibilityOff();
    m_bottomRuler->SetTickLength( 10 );
    m_bottomRuler->GetProperty()->SetColor( 0 , 1 , 0 );
    this->getRenderer()->AddActor2D( m_bottomRuler );

    updateAnnotations();
}

void Q2DViewer::mapOrientationStringToAnnotation()
{
    QString orientation = m_mainVolume->getVolumeSourceInformation()->getPatientOrientationString() ;
    QString revertedOrientation = m_mainVolume->getVolumeSourceInformation()->getRevertedPatientOrientationString() ;
    
    QStringList list = orientation.split(",");
    QStringList revertedList = revertedOrientation.split(",");
    
    if( list.size() > 1 )
    {
        // 0:Esquerra , 1:Abaix , 2:Dreta , 3:A dalt
        if( m_lastView == Axial )
        {
            m_patientOrientationTextActor[0]->SetInput( qPrintable( revertedList.at(0) ) );
            m_patientOrientationTextActor[2]->SetInput( qPrintable( list.at(0) ) );
            m_patientOrientationTextActor[1]->SetInput( qPrintable( list.at(1) ) );
            m_patientOrientationTextActor[3]->SetInput( qPrintable( revertedList.at(1) ) );
        }
        else if( m_lastView == Sagittal )
        {
            m_patientOrientationTextActor[0]->SetInput( qPrintable( revertedList.at(1) ) );
            m_patientOrientationTextActor[2]->SetInput( qPrintable( list.at(1) ) );
            m_patientOrientationTextActor[1]->SetInput( qPrintable( revertedList.at(2) ) );
            m_patientOrientationTextActor[3]->SetInput( qPrintable( list.at(2) ) );
        }
        else if( m_lastView == Coronal )
        {
            m_patientOrientationTextActor[0]->SetInput( qPrintable( revertedList.at(0) ) );
            m_patientOrientationTextActor[2]->SetInput( qPrintable( list.at(0) ) );
            m_patientOrientationTextActor[1]->SetInput( qPrintable( revertedList.at(2) ) );
            m_patientOrientationTextActor[3]->SetInput( qPrintable( list.at(2) ) );
        }
    }
    else
    {
        // la info no existeix
    }
}

void Q2DViewer::updateAnnotations()
{
    if( m_enabledAnnotations & Q2DViewer::ReferenceAnnotation )
    {
        m_patientOrientationTextActor[0]->VisibilityOn();
        m_patientOrientationTextActor[1]->VisibilityOn();
        m_patientOrientationTextActor[2]->VisibilityOn();
        m_patientOrientationTextActor[3]->VisibilityOn();
    }
    else
    {
        m_patientOrientationTextActor[0]->VisibilityOff();
        m_patientOrientationTextActor[1]->VisibilityOff();
        m_patientOrientationTextActor[2]->VisibilityOff();
        m_patientOrientationTextActor[3]->VisibilityOff();
    }

    if( m_enabledAnnotations & Q2DViewer::RuleAnnotation )
    {
        m_bottomRuler->VisibilityOn();
        m_sideRuler->VisibilityOn();
    }
    else
    {
        m_bottomRuler->VisibilityOff();
        m_sideRuler->VisibilityOff();
    }
    
    if( m_enabledAnnotations & Q2DViewer::WindowLevelAnnotation )
    {
        m_textAnnotation->VisibilityOn();
    }
    else
    {
        m_textAnnotation->VisibilityOff();
    }
}

void Q2DViewer::initInformationText()
{
    m_lowerLeftText = tr("Slice: %1/%2")
                .arg( m_currentSlice )
                .arg( m_viewer->GetSliceMax() );
                
    m_upperLeftText = tr("Image Size: %1 x %2\nView Size: %3 x %4\nWW: %5 WL: %6 ")
                .arg( m_size[0] )
                .arg( m_size[1] )
                .arg( m_viewer->GetRenderWindow()->GetSize()[0] )
                .arg( m_viewer->GetRenderWindow()->GetSize()[1] )
                .arg( m_viewer->GetColorWindow() )
                .arg( m_viewer->GetColorLevel() );

    QString studyDate = m_mainVolume->getVolumeSourceInformation()->getStudyDate();
    QString year = studyDate.mid( 0 , 4 );
    QString month = studyDate.mid( 4 , 2 );
    QString day = studyDate.mid( 6 , 2 );
    studyDate = day + QString( "/" ) + month + QString( "/" ) + year;

    QString studyTime = m_mainVolume->getVolumeSourceInformation()->getStudyTime();
    QString hour = studyTime.mid( 0 , 2 );
    QString minute = studyTime.mid( 2 , 2 );
    QString second = studyTime.mid( 4 , 2 );
    studyTime = hour + QString( ":" ) + minute + QString( ":" ) + second;
    
    m_upperRightText = tr("%1\n%2\n%3\nAcc:%4\n%5\n%6")
                .arg( m_mainVolume->getVolumeSourceInformation()->getInstitutionName() )
                .arg( m_mainVolume->getVolumeSourceInformation()->getPatientName() )
                .arg( m_mainVolume->getVolumeSourceInformation()->getPatientID() )
                .arg( m_mainVolume->getVolumeSourceInformation()->getAccessionNumber() )
                .arg( studyDate )
                .arg( studyTime );
                
    m_lowerRightText = tr("%1")
                    .arg( m_mainVolume->getVolumeSourceInformation()->getProtocolName() );
    
    m_textAnnotation->SetText( 0 , m_lowerLeftText.toAscii() );
    m_textAnnotation->SetText( 1 , m_lowerRightText.toAscii() );
    m_textAnnotation->SetText( 2 , m_upperLeftText.toAscii() );
    m_textAnnotation->SetText( 3 , m_upperRightText.toAscii() );
    
    m_textAnnotation->SetImageActor( m_viewer->GetImageActor() );
    m_textAnnotation->SetWindowLevel( m_viewer->GetWindowLevel() );
    m_textAnnotation->ShowSliceAndImageOn();
    
    m_viewer->GetRenderer()->AddActor2D( m_textAnnotation );
}

void Q2DViewer::displayInformationText( bool display )
{
    if( display )
    {
        m_textAnnotation->VisibilityOn();
        connect( this , SIGNAL( infoChanged() ) , this , SLOT( updateInformationText() ) );
    }
    else
    {
        m_textAnnotation->VisibilityOff();
        disconnect( this , SIGNAL( infoChanged() ) , this , SLOT( updateInformationText() ) );
    }
}

void Q2DViewer::updateVoxelInformation()
{   
    vtkRenderWindowInteractor* interactor = m_vtkWidget->GetRenderWindow()->GetInteractor();
    // agafem el punt que est� apuntant el ratol� en aquell moment \TODO podr�em passar-li el 4t par�matre opcional (vtkPropCollection) per indicar que nom�s agafi de l'ImageActor, per� no sembla que suigui necessari realment i que si fa pick d'un altre actor 2D no passa res
    m_picker->PickProp( interactor->GetEventPosition()[0], interactor->GetEventPosition()[1], m_viewer->GetRenderer() );
    // calculem el pixel trobat
    double q[3], imageValue;    
    m_picker->GetPickPosition( q );
    //     this->m_modelPointFromCursor.setValues( q );
    int found = 0;
    // quan dona una posici� de (0,0,0) �s que estem fora de l'actor
    if( !( q[0] == 0 && q[1] == 0 && q[2] == 0) )
    {
        double tolerance;
        int subCellId;
        double parametricCoordinates[3], interpolationWeights[8];
        
        vtkPointData *pointData = m_mainVolume->getVtkData()->GetPointData();
        vtkPointData* outPointData = vtkPointData::New();
        outPointData->InterpolateAllocate( pointData , 1 , 1 );
    
        // Use tolerance as a function of size of source data
        tolerance = m_mainVolume->getVtkData()->GetLength();
        tolerance = tolerance ? tolerance*tolerance / 1000.0 : 0.001;
    
        // Find the cell that contains q and get it
        vtkCell *cell = m_mainVolume->getVtkData()->FindAndGetCell( q , NULL , -1 , tolerance , subCellId , parametricCoordinates , interpolationWeights );
        if ( cell )
        {
            // Interpolate the point data
            outPointData->InterpolatePoint( pointData , 0 , cell->PointIds , interpolationWeights );
            imageValue = outPointData->GetScalars()->GetTuple1(0);
            found = 1;
        }
        outPointData->Delete();
    }
    if( !found )
    {
        updateCursor( -1, -1, -1, -1 );
        m_voxelInformationCaption->VisibilityOff();
    }
    else
    {
        updateCursor( q[0], q[1], q[2], imageValue );
        m_voxelInformationCaption->VisibilityOn();
        m_voxelInformationCaption->SetAttachmentPoint( q );
        m_voxelInformationCaption->SetCaption( qPrintable( QString("(%1,%2,%3):%4").arg(m_currentCursorPosition[0]).arg(m_currentCursorPosition[1]).arg(m_currentCursorPosition[2]).arg(m_currentImageValue) ) );
    }
    this->getInteractor()->Render();
}

void Q2DViewer::onMouseMove()
{
    updateVoxelInformation();
    switch( m_currentTool )
    {
    case Q2DViewer::Manipulate:
        int x = this->getInteractor()->GetEventPosition()[0];
        int y = this->getInteractor()->GetEventPosition()[1];
        double toWorld[3];
        this->computeDisplayToWorld( this->getRenderer() , x, y , 0 , toWorld );
        emit mouseMove( toWorld[0] , toWorld[1] );
    break;

    default:
    break;
    }
}

void Q2DViewer::onLeftButtonDown()
{
    switch( m_currentTool )
    {
    case Q2DViewer::Manipulate:
        int x, y;
        x = this->getInteractor()->GetEventPosition()[0];
        y = this->getInteractor()->GetEventPosition()[1];
        double toWorld[3];
        this->computeDisplayToWorld( this->getRenderer() , x, y , 0 , toWorld );
        emit leftButtonDown( toWorld[0] , toWorld[1] );
    break;

    default:
    break;
    }
}

void Q2DViewer::onLeftButtonUp()
{
    switch( m_currentTool )
    {
    case Q2DViewer::Manipulate:
        int x, y;
        x = this->getInteractor()->GetEventPosition()[0];
        y = this->getInteractor()->GetEventPosition()[1];
        double toWorld[3];
        this->computeDisplayToWorld( this->getRenderer() , x, y , 0 , toWorld );
        emit leftButtonUp( toWorld[0] , toWorld[1] );
    break;

    default:
    break;
    }
}

void Q2DViewer::onRightButtonDown()
{
    switch( m_currentTool )
    {
    case Q2DViewer::Manipulate:
        int x, y;
        x = this->getInteractor()->GetEventPosition()[0];
        y = this->getInteractor()->GetEventPosition()[1];
        double toWorld[3];
        this->computeDisplayToWorld( this->getRenderer() , x, y , 0 , toWorld );
        emit rightButtonDown( toWorld[0] , toWorld[1] );
    break;

    default:
    break;
    }
}

void Q2DViewer::onRightButtonUp()
{
    switch( m_currentTool )
    {
    case Q2DViewer::Manipulate:
        int x, y;
        x = this->getInteractor()->GetEventPosition()[0];
        y = this->getInteractor()->GetEventPosition()[1];
        double toWorld[3];
        this->computeDisplayToWorld( this->getRenderer() , x, y , 0 , toWorld );
        emit rightButtonUp( toWorld[0] , toWorld[1] );
    break;

    default:
    break;
    }
}

void Q2DViewer::eventHandler( vtkObject *obj, unsigned long event, void *client_data, vtkCommand *command )
{
//     static double factor = this->getRenderer()->GetActiveCamera()->GetParallelScale();
//     double fac = this->getRenderer()->GetActiveCamera()->GetParallelScale() - factor;
//     if( fac != 0.0 )
//     {
//         factor = this->getRenderer()->GetActiveCamera()->GetParallelScale();
//     }
    updateRulers();
    // fer el que calgui per cada tipus d'event
    switch( event )
    {
    case vtkCommand::MouseMoveEvent:    
        onMouseMove();
    break;

    case vtkCommand::LeftButtonPressEvent:
        m_lastButtonPressed = Q2DViewer::LeftButton;
        onLeftButtonDown();
    break;

    case vtkCommand::LeftButtonReleaseEvent:
        m_lastButtonPressed = Q2DViewer::LeftButton;
        onLeftButtonUp();
    break;
    
    case vtkCommand::RightButtonPressEvent:
        m_lastButtonPressed = Q2DViewer::RightButton;
        onRightButtonDown();
    break;

    case vtkCommand::RightButtonReleaseEvent:
        m_lastButtonPressed = Q2DViewer::RightButton;
        onRightButtonUp();
    break;

    default:
    break;
    }
}

void Q2DViewer::contextMenuRelease( vtkObject* object , unsigned long event, void *client_data, vtkCommand *command )
{
    // Extret dels exemples de vtkEventQtSlotConnect
    // get interactor
    vtkRenderWindowInteractor* iren = vtkRenderWindowInteractor::SafeDownCast(object);
    // consume event so the interactor style doesn't get it
    command->AbortFlagOn();
    // Obtenim la posici� de l'event (moure el mouse, en aquest cas)
    int eventPosition[2];
    iren->GetEventPosition( eventPosition );
    int* size = iren->GetSize();
    // remember to flip y
    QPoint pt = QPoint( eventPosition[0], size[1]-eventPosition[1]);

    // aquesta posici� no �s del tot bona ja que no s�n les coordenades globals, sin o de finestra
    QMenu contextMenu( this );
    contextMenu.addAction( m_resetAction );
    
    // map to global
    QPoint global_pt = contextMenu.parentWidget()->mapToGlobal( pt );
    contextMenu.exec( global_pt );
}

void Q2DViewer::setupInteraction()
{   
    // configurem l'Image Viewer i el qvtkWidget
    // aquesta crida obliga a que hi hagi un input abans, sin� el pipeline del vtkImageViewer ens d�na error perqu� no t� cap actor creat \TODO aquesta crida hauria d'anar aqu� o nom�s despr�s del primer setInput?
    m_vtkWidget->SetRenderWindow( m_viewer->GetRenderWindow() );
    m_viewer->SetupInteractor( m_vtkWidget->GetRenderWindow()->GetInteractor() );
    
    m_vtkQtConnections = vtkEventQtSlotConnect::New();
    m_vtkWidget->GetRenderWindow()->GetInteractor()->SetPicker( m_picker );

// men� contextual
//     m_vtkQtConnections->Connect( m_vtkWidget->GetRenderWindow()->GetInteractor(),
//                       QVTKWidget::ContextMenuEvent,//vtkCommand::RightButtonPressEvent,
//                        this,
//                        SLOT( contextMenuRelease(vtkObject*,unsigned long,void*, vtkCommand *) ) );

    // despatxa qualsevol event-> tools                       
    m_vtkQtConnections->Connect( m_vtkWidget->GetRenderWindow()->GetInteractor(), vtkCommand::AnyEvent, this, SLOT( eventHandler(vtkObject*,unsigned long,void *, vtkCommand *) ) );

    WindowLevelCallback * wlcbk = WindowLevelCallback::New();
    wlcbk->m_viewer = this;
    
    m_viewer->GetInteractorStyle()->AddObserver( vtkCommand::StartWindowLevelEvent , wlcbk );
    m_viewer->GetInteractorStyle()->AddObserver( vtkCommand::WindowLevelEvent , wlcbk );
    m_viewer->GetInteractorStyle()->AddObserver( vtkCommand::EndWindowLevelEvent , wlcbk );
    
//     // anulem el window levelling manual
// //     m_viewer->GetInteractorStyle()->RemoveObservers( vtkCommand::StartWindowLevelEvent );
// //     m_viewer->GetInteractorStyle()->RemoveObservers( vtkCommand::WindowLevelEvent );
// //     m_viewer->GetInteractorStyle()->RemoveObservers( vtkCommand::ResetWindowLevelEvent );
//     // aquests observers estan de prova

// 
// //     m_viewer->GetInteractorStyle()->AddObserver( vtkCommand::LeftButtonPressEvent , wlcbk );
// 
//     m_viewer->GetInteractorStyle()->AddObserver( vtkCommand::RightButtonPressEvent , wlcbk , 0 );

//     ZoomTool *zoom = new ZoomTool( m_viewer->GetInteractorStyle() );
//     WindowLevelTool *wl = new WindowLevelTool( m_viewer->GetInteractorStyle() );
//     WindowLevelTool *wl = new WindowLevelTool();
//     wl->setup( m_viewer->GetInteractorStyle() , this );
}

void Q2DViewer::initializeRulers()
{
    m_anchoredRulerCoordinates = vtkCoordinate::New();
    m_anchoredRulerCoordinates->SetCoordinateSystemToView();
    m_anchoredRulerCoordinates->SetValue( -0.95 , -0.9 , -0.95 );

    m_sideRuler->GetPositionCoordinate()->SetCoordinateSystemToWorld();
    m_sideRuler->GetPosition2Coordinate()->SetCoordinateSystemToWorld();

    m_bottomRuler->GetPositionCoordinate()->SetCoordinateSystemToWorld();
    m_bottomRuler->GetPosition2Coordinate()->SetCoordinateSystemToWorld();
    
    updateRulers();
}

void Q2DViewer::updateRulers()
{
    double *anchoredCoordinates = m_anchoredRulerCoordinates->GetComputedWorldValue( this->getRenderer() );
    switch( m_lastView )
    {
    case Axial:
        m_sideRuler->GetPositionCoordinate()->SetValue( anchoredCoordinates[0] , m_rulerExtent[3] , 0.0 );
        m_sideRuler->GetPosition2Coordinate()->SetValue( anchoredCoordinates[0] , m_rulerExtent[2] , 0.0 );

        m_bottomRuler->GetPositionCoordinate()->SetValue( m_rulerExtent[1] , anchoredCoordinates[1]  , 0.0 );
        m_bottomRuler->GetPosition2Coordinate()->SetValue( m_rulerExtent[0] , anchoredCoordinates[1] , 0.0  );
    break;

    case Sagittal:
        m_sideRuler->GetPositionCoordinate()->SetValue( 0.0 , anchoredCoordinates[1] , m_rulerExtent[4] );
        m_sideRuler->GetPosition2Coordinate()->SetValue( 0.0 , anchoredCoordinates[1] , m_rulerExtent[5] );

        m_bottomRuler->GetPositionCoordinate()->SetValue( 0.0 , m_rulerExtent[1] , anchoredCoordinates[2] );
        m_bottomRuler->GetPosition2Coordinate()->SetValue( 0.0 , m_rulerExtent[0] , anchoredCoordinates[2] );
    break;

    case Coronal:
        m_sideRuler->GetPositionCoordinate()->SetValue( anchoredCoordinates[0] , 0.0 , m_rulerExtent[4] );
        m_sideRuler->GetPosition2Coordinate()->SetValue( anchoredCoordinates[0] , 0.0 , m_rulerExtent[5] );

        m_bottomRuler->GetPositionCoordinate()->SetValue( m_rulerExtent[1] , 0.0 , anchoredCoordinates[2] );
        m_bottomRuler->GetPosition2Coordinate()->SetValue( m_rulerExtent[0] , 0.0 , anchoredCoordinates[2] );
    break;
    }
}

void Q2DViewer::setInput( Volume* volume )
{
    if( volume == 0 )
        return;
    m_mainVolume = volume;
    m_viewer->SetInput( m_mainVolume->getVtkData() );
    // ajustem el window Level per defecte
    m_defaultWindow = m_mainVolume->getVolumeSourceInformation()->getWindow();
    m_defaultLevel = m_mainVolume->getVolumeSourceInformation()->getLevel();
    if( m_defaultWindow == 0.0 && m_defaultLevel == 0.0 )
    {
        double * range = m_mainVolume->getVtkData()->GetScalarRange();
        m_defaultWindow = fabs( range[1] - range[0] );
        m_defaultLevel = ( range[1] + range[0] )/ 2.0;
    }
    
    m_mainVolume->getDimensions( m_size );
    double origin[3], spacing[3];
    m_mainVolume->getOrigin( origin );
    m_mainVolume->getSpacing( spacing );
    m_rulerExtent[0] = origin[0];
    m_rulerExtent[1] = origin[0] + m_size[0]*spacing[0];
    m_rulerExtent[2] = origin[1];
    m_rulerExtent[3] = origin[1] + m_size[1]*spacing[1];
    m_rulerExtent[4] = origin[2];
    m_rulerExtent[5] = origin[2] + m_size[2]*spacing[2];
    
    initializeRulers();
    initInformationText();

    // \TODO s'ha de cridar cada cop que posem dades noves o nom�s el primer cop?
    setupInteraction();
}

void Q2DViewer::setOverlayInput( Volume* volume )
{
    m_overlayVolume = volume;
    
    vtkImageCheckerboard *imageCheckerBoard = vtkImageCheckerboard::New();
    vtkImageBlend *blender;
    
    vtkImageRectilinearWipe *wipe = vtkImageRectilinearWipe::New();
    
    switch( m_overlay )
    {
    case CheckerBoard:
        
        imageCheckerBoard->SetInput1( m_mainVolume->getVtkData() );
        imageCheckerBoard->SetInput2( m_overlayVolume->getVtkData() );
        imageCheckerBoard->SetNumberOfDivisions( m_divisions );
        // actualitzem el viewer
        m_viewer->SetInputConnection( imageCheckerBoard->GetOutputPort() ); // li donem el m_imageCheckerboard com a input
        // \TODO haur�em d'actualitzar valors que es calculen al setInput!
    break;
    
    case Blend:
        blender = vtkImageBlend::New();
        blender->SetInput(m_mainVolume->getVtkData());
        blender->AddInput(m_overlayVolume->getVtkData());
        blender->SetOpacity( 1, 0.5 );
        blender->SetOpacity( 2, 0.5 );
        m_viewer->SetInputConnection( blender->GetOutputPort() ); // li donem el blender com a input
        // \TODO haur�em d'actualitzar valors que es calculen al setInput!
    break;
    
    case RectilinearWipe:
        wipe->SetInput( 0 , m_mainVolume->getVtkData() );
        wipe->SetInput( 1 , m_overlayVolume->getVtkData() );
        wipe->SetPosition(20,20);
        wipe->SetWipeToUpperLeft();        
        m_viewer->SetInput( wipe->GetOutput() );
        // \TODO haur�em d'actualitzar valors que es calculen al setInput!
    break;    
    }
}

void Q2DViewer::render()
{
    // si tenim dades
    if( m_mainVolume )
    {        
        // li donem el window/level correcte \TODO no creiem convenient que aquesta crida es faci aqu�
//         resetWindowLevelToDefault();
        // Aix� �s necessari perqu� la imatge es rescali a les mides de la finestreta
        m_viewer->GetRenderer()->ResetCamera();
        updateView();
    }
    // mostrar error/av�s si no hi ha dades per visualitzar?
}

void Q2DViewer::setView( ViewType view )
{    
    m_lastView = view;
    updateView();
}

void Q2DViewer::updateView()
{
    if( m_viewer->GetInput() )
    {
        switch( m_lastView )
        {
        case Axial:
            m_viewer->SetSliceOrientationToXY();
            vtkCamera *cam;
            cam = this->getRenderer() ? this->getRenderer()->GetActiveCamera() : NULL;
            if ( cam )
            {
                cam->SetFocalPoint(0,0,0);
                cam->SetViewUp(0,-1,0);
                cam->SetPosition(0,0,-1);
                this->getRenderer()->ResetCamera();
            }
            m_size[0] = m_mainVolume->getDimensions()[0];
            m_size[1] = m_mainVolume->getDimensions()[1];
            m_size[2] = m_mainVolume->getDimensions()[2];
        break;
        
        case Sagittal:
            m_viewer->SetSliceOrientationToYZ();
            //\TODO hauria de ser a partir de main_volume o a partir de l'output del viewer
            m_size[0] = m_mainVolume->getDimensions()[1];
            m_size[1] = m_mainVolume->getDimensions()[2];
            m_size[2] = m_mainVolume->getDimensions()[0];
        break;
    
        case Coronal:
            m_viewer->SetSliceOrientationToXZ();
            //\TODO hauria de ser a partir de main_volume o a partir de l'output del viewer
            m_size[0] = m_mainVolume->getDimensions()[0];
            m_size[1] = m_mainVolume->getDimensions()[2];
            m_size[2] = m_mainVolume->getDimensions()[1];
        break;
    
        default:
        // podem posar en Axial o no fer res
            m_viewer->SetSliceOrientationToXY();
        break;
        }
        // cada cop que canviem de llesca posarem per defecte la llesca del mig d'aquella vista
        setSlice( m_viewer->GetSliceRange()[1]/2 );
        mapOrientationStringToAnnotation();
        updateWindowSizeAnnotation();
        updateRulers();
        this->getInteractor()->Render();
    }
    else
    {
        WARN_LOG( "Intentant canviar de vista sense haver donat un input abans..." );
    }
}

void Q2DViewer::setSlice( int value )
{
    m_currentSlice = value;
    emit sliceChanged( m_currentSlice );

    if( m_currentSlice <= m_viewer->GetSliceRange()[1] && m_currentSlice >= m_viewer->GetSliceRange()[0] )
    {
        m_viewer->SetSlice( m_currentSlice );
    }
    updateSliceAnnotation();
}

void Q2DViewer::resizeEvent( QResizeEvent *resize )
{
    updateWindowSizeAnnotation();
}

void Q2DViewer::setWindowLevel( double window , double level )
{
    if( m_viewer && m_mainVolume )
    {
        m_viewer->SetColorLevel( level );
        m_viewer->SetColorWindow( window );
        m_upperLeftText = tr("Image Size: %1 x %2\nView Size: %3 x %4\nWW: %5 WL: %6 ")
                .arg( m_size[0] )
                .arg( m_size[1] )
                .arg( m_viewer->GetRenderWindow()->GetSize()[0] )
                .arg( m_viewer->GetRenderWindow()->GetSize()[1] )
                .arg( m_viewer->GetColorWindow() )
                .arg( m_viewer->GetColorLevel() );
        m_textAnnotation->SetText( 2 , m_upperLeftText.toAscii() );
        getInteractor()->Render();
    }
}

void Q2DViewer::getWindowLevel( double wl[2] )
{
    if( m_viewer && m_mainVolume )
    {
        wl[0] = m_defaultWindow;
        wl[1] = m_defaultLevel;
    }
}

void Q2DViewer::resetWindowLevelToDefault()
{
// aix� ens d�na un level/level "maco" per defecte
// situem el level al mig i donem un window complet de tot el rang
    if( m_mainVolume )
    {
        m_viewer->SetColorWindow( m_defaultWindow );
        m_viewer->SetColorLevel( m_defaultLevel );

        this->getInteractor()->Render();
        updateWindowLevelAnnotation();
    }
    // mostrar av�s/error si no hi ha volum?
}

void Q2DViewer::updateWindowLevelAnnotation()
{
    m_upperLeftText = tr("Image Size: %1 x %2\nView Size: %3 x %4\nWW: %5 WL: %6 ")
                .arg( m_size[0] )
                .arg( m_size[1] )
                .arg( m_viewer->GetRenderWindow()->GetSize()[0] )
                .arg( m_viewer->GetRenderWindow()->GetSize()[1] )
                .arg( m_viewer->GetColorWindow() )
                .arg( m_viewer->GetColorLevel() );
    m_textAnnotation->SetText( 2 , m_upperLeftText.toAscii() );
    emit windowLevelChanged( m_viewer->GetColorWindow() , m_viewer->GetColorLevel() );
}

void Q2DViewer::updateSliceAnnotation()
{
    m_lowerLeftText = tr("Slice: %1/%2")
                .arg( m_currentSlice )
                .arg( m_viewer->GetSliceMax() );
    m_textAnnotation->SetText( 0 , m_lowerLeftText.toAscii() );
    this->getInteractor()->Render();
}

void Q2DViewer::updateWindowSizeAnnotation()
{
    m_upperLeftText = tr("Image Size: %1 x %2\nView Size: %3 x %4\nWW: %5 WL: %6 ")
                .arg( m_size[0] )
                .arg( m_size[1] )
                .arg( m_viewer->GetRenderWindow()->GetSize()[0] )
                .arg( m_viewer->GetRenderWindow()->GetSize()[1] )
                .arg( m_viewer->GetColorWindow() )
                .arg( m_viewer->GetColorLevel() );
    m_textAnnotation->SetText( 2 , m_upperLeftText.toAscii() );
}
    
void Q2DViewer::setDivisions( int x , int y , int z )
{
    m_divisions[0] = x;
    m_divisions[1] = y;
    m_divisions[2] = z;
}

void Q2DViewer::setDivisions( int data[3] )
{
    m_divisions[0] = data[0];
    m_divisions[1] = data[1];
    m_divisions[2] = data[2];
}

int* Q2DViewer::getDivisions( void )
{
    return m_divisions;
}

void Q2DViewer::getDivisions( int data[3] )
{
    data[0] = m_divisions[0];
    data[1] = m_divisions[1];
    data[2] = m_divisions[2];
}

void Q2DViewer::saveAll( const char *baseName , FileType extension )
{
    switch( extension )
    {
    case PNG:
    break;
    
    case JPEG:
    break;

    case TIFF:
    break;
     
    case DICOM:
    break;

    case META:
    break;
    
    case PNM:
    break;

    case BMP:
    break;
    }
}

void Q2DViewer::saveCurrent( const char *baseName , FileType extension )
{
    m_windowToImageFilter->Update();
    m_windowToImageFilter->Modified();
    vtkImageData *image = m_windowToImageFilter->GetOutput();
    switch( extension )
    {
    case PNG:
        vtkImageWriter *pngWriter = vtkPNGWriter::New();
        pngWriter->SetInput( image );
        pngWriter->SetFilePattern( "%s-%d.png" );
        pngWriter->SetFilePrefix( baseName );
        pngWriter->Write();
    break;
    
    case JPEG:
        vtkImageWriter *jpegWriter = vtkJPEGWriter::New();
        jpegWriter->SetInput( image );
        jpegWriter->SetFilePattern( "%s-%d.jpg" );
        jpegWriter->SetFilePrefix( baseName );
        jpegWriter->Write();
    break;
// \TODO el format tiff fa petar al desar, mirar si �s problema de compatibilitat del sistema o de les pr�pies vtk
    case TIFF:
        vtkImageWriter *tiffWriter = vtkTIFFWriter::New();
        tiffWriter->SetInput( image );
        tiffWriter->SetFilePattern( "%s-%d.tif" );
        tiffWriter->SetFilePrefix( baseName );
        tiffWriter->Write();
    break;

    case PNM:
        vtkImageWriter *pnmWriter = vtkPNMWriter::New();
        pnmWriter->SetInput( image );
        pnmWriter->SetFilePattern( "%s-%d.pnm" );
        pnmWriter->SetFilePrefix( baseName );
        pnmWriter->Write();
    break;

    case BMP:
        vtkImageWriter *bmpWriter = vtkBMPWriter::New();
        bmpWriter->SetInput( image );
        bmpWriter->SetFilePattern( "%s-%d.bmp" );
        bmpWriter->SetFilePrefix( baseName );
        bmpWriter->Write();
    break;
    
    case DICOM:
    break;

    case META:
    break;

    }

}

};  // end namespace udg 

