/*==============================================================================

 Distributed under the OSI-approved BSD 3-Clause License.

  Copyright (c) Oslo University Hospital. All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

  * Neither the name of Oslo University Hospital nor the names of Contributors
    may be used to endorse or promote products derived from this
    software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  This file was originally developed by Rafael Palomar (The Intervention Centre,
  Oslo University Hospital) and was supported by The Research Council of Norway
  through the ALive project (grant nr. 311393).

==============================================================================*/
#include "vtkSlicerLiverResectionsLogic.h"

#include <vtkMRMLMarkupsSlicingContourNode.h>
#include <vtkMRMLMarkupsDistanceContourNode.h>
#include <vtkMRMLMarkupsDisplayNode.h>
#include <vtkMRMLLiverResectionNode.h>
#include <vtkMRMLLiverResectionDisplayNode.h>

// MRML includes
#include <vtkMRMLScene.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLSegmentationNode.h>

// VTK includes
#include <vtkObjectFactory.h>
#include <vtkCollection.h>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerLiverResectionsLogic);

//---------------------------------------------------------------------------
vtkSlicerLiverResectionsLogic::vtkSlicerLiverResectionsLogic()
{

}

//---------------------------------------------------------------------------
vtkSlicerLiverResectionsLogic::~vtkSlicerLiverResectionsLogic() = default;

//---------------------------------------------------------------------------
void vtkSlicerLiverResectionsLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//---------------------------------------------------------------------------
void vtkSlicerLiverResectionsLogic::RegisterNodes()
{
  assert(this->GetMRMLScene() != nullptr);
  vtkMRMLScene *scene = this->GetMRMLScene();

  // Nodes
  scene->RegisterNodeClass(vtkSmartPointer<vtkMRMLLiverResectionNode>::New());
  scene->RegisterNodeClass(vtkSmartPointer<vtkMRMLLiverResectionDisplayNode>::New());
}

//---------------------------------------------------------------------------
void vtkSlicerLiverResectionsLogic::ObserveMRMLScene()
{
  if (!this->GetMRMLScene())
    {
    return;
    }

 this->Superclass::ObserveMRMLScene();
}

//---------------------------------------------------------------------------
void vtkSlicerLiverResectionsLogic::OnMRMLSceneNodeAdded(vtkMRMLNode* node)
{
  Superclass::OnMRMLSceneNodeAdded(node);

  std::cout << "1" << std::endl;

  // check for nullptr
  if (!node)
    {
    return;
    }

  std::cout << "2" << std::endl;
  // Check wether it is a relevant node to handle
  vtkMRMLLiverResectionNode *resectionNode =
    vtkMRMLLiverResectionNode::SafeDownCast(node);
  if(!resectionNode)
    {
    return;
    }

  std::cout << "3" << std::endl;
  vtkMRMLMarkupsNode *markupsNode;
  switch(resectionNode->GetResectionInitialization())
    {
    case vtkMRMLLiverResectionNode::Flat:
      markupsNode = this->AddResectionPlane(resectionNode);
      break;

    case vtkMRMLLiverResectionNode::Curved:
      markupsNode = this->AddResectionContour(resectionNode);
      break;
    }

  this->ResectionsMarkupsMap[resectionNode] = markupsNode;
}

//---------------------------------------------------------------------------
vtkMRMLMarkupsSlicingContourNode* vtkSlicerLiverResectionsLogic::AddResectionPlane(vtkMRMLLiverResectionNode *resectionNode) const
{
  auto mrmlScene = this->GetMRMLScene();
  if (!mrmlScene)
    {
      vtkErrorMacro("Error in AddResectionPlane: no valid MRML scene.");
      return nullptr;
    }

  if (!resectionNode->GetTargetOrgan())
    {
      vtkErrorMacro("Error in AddResectionPlane: invalid internal target parenchyma.");
      return nullptr;
    }

  auto targetParenchymaPolyData = resectionNode->GetTargetOrgan()->GetPolyData();
  if (!targetParenchymaPolyData)
    {
      vtkErrorMacro("Error in AddResectionPlane: target liver model does not contain valid polydata.");
      return nullptr;
    }

  // Computing the position of the initial points
  const double *bounds = targetParenchymaPolyData->GetBounds();
  auto p1 = vtkVector3d(bounds[0],(bounds[3]-bounds[2])/2.0, (bounds[5]-bounds[4])/2.0);
  auto p2 = vtkVector3d(bounds[1],(bounds[3]-bounds[2])/2.0, (bounds[5]-bounds[4])/2.0);

  auto slicingContourNode = vtkSmartPointer<vtkMRMLMarkupsSlicingContourNode>::New();
  slicingContourNode->AddControlPoint(p1);
  slicingContourNode->AddControlPoint(p2);
  slicingContourNode->SetTarget(resectionNode->GetTargetOrgan());

  auto slicingContourDisplayNode = vtkSmartPointer<vtkMRMLMarkupsDisplayNode>::New();
  slicingContourDisplayNode->PropertiesLabelVisibilityOff();
  slicingContourDisplayNode->SetSnapMode(vtkMRMLMarkupsDisplayNode::SnapModeUnconstrained);

  mrmlScene->AddNode(slicingContourDisplayNode);
  slicingContourNode->SetAndObserveDisplayNodeID(slicingContourDisplayNode->GetID());
  mrmlScene->AddNode(slicingContourNode);

  return slicingContourNode;
}

//---------------------------------------------------------------------------
vtkMRMLMarkupsDistanceContourNode* vtkSlicerLiverResectionsLogic::AddResectionContour(vtkMRMLLiverResectionNode *resectionNode) const
{
  auto mrmlScene = this->GetMRMLScene();
  if (!mrmlScene)
    {
    vtkErrorMacro("Error in AddResectionContour: no valid MRML scene.");
    return nullptr;
    }

  if (!resectionNode->GetSegmentationNode())
    {
     vtkErrorMacro("Error in AddResectionContour: no valid segmentation node.");
     return nullptr;
    }

  if (!resectionNode->GetTargetOrgan())
    {
      vtkErrorMacro("Error in AddResectionContour: no valid target parenchyma node.");
      return nullptr;
    }

  if (resectionNode->GetTargetTumors().empty())
    {
      vtkErrorMacro("Error in AddResectionContour: no valid target tumor.");
      return nullptr;
    }

  // Computing the position of the initial points
  const double *bounds = resectionNode->GetTargetOrgan()->GetPolyData()->GetBounds();

  auto p1 = vtkVector3d(bounds[0],(bounds[3]-bounds[2])/2.0, (bounds[5]-bounds[4])/2.0);
  auto p2 = vtkVector3d((bounds[1]-bounds[0])/2.0,(bounds[3]-bounds[2])/2.0, (bounds[5]-bounds[4])/2.0);

  auto distanceContourNode = vtkSmartPointer<vtkMRMLMarkupsDistanceContourNode>::New();
  distanceContourNode->AddControlPoint(p1);
  distanceContourNode->AddControlPoint(p2);
  distanceContourNode->SetTarget(resectionNode->GetTargetOrgan());

  auto distanceContourDisplayNode = vtkSmartPointer<vtkMRMLMarkupsDisplayNode>::New();
  distanceContourDisplayNode->PropertiesLabelVisibilityOff();
  distanceContourDisplayNode->SetSnapMode(vtkMRMLMarkupsDisplayNode::SnapModeUnconstrained);

  mrmlScene->AddNode(distanceContourDisplayNode);
  distanceContourNode->SetAndObserveDisplayNodeID(distanceContourDisplayNode->GetID());
  mrmlScene->AddNode(distanceContourNode);

  // vtkSmartPointer<vtkMRMLLiverResectionNode> node =
  //   vtkSmartPointer<vtkMRMLLiverResectionNode>::New();

  // node->SetSegmentationNode(segmentationNode);
  // node->SetTargetOrganID(targetParenchymaModelNode->GetID());

  // for (int i=0; i<targetTumors->GetNumberOfItems(); ++i)
  //   {
  //     vtkMRMLNode *tumorNode = vtkMRMLNode::SafeDownCast(targetTumors->GetItemAsObject(i));
  //     if (!tumorNode)
  //       {
  //         continue;
  //       }

  //     node->AddTargetTumorID(tumorNode->GetID());
  //   }

  // mrmlScene->AddNode(node);
}
