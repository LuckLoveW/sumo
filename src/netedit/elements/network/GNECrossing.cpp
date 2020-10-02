/****************************************************************************/
// Eclipse SUMO, Simulation of Urban MObility; see https://eclipse.org/sumo
// Copyright (C) 2001-2020 German Aerospace Center (DLR) and others.
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License 2.0 which is available at
// https://www.eclipse.org/legal/epl-2.0/
// This Source Code may also be made available under the following Secondary
// Licenses when the conditions for such availability set forth in the Eclipse
// Public License 2.0 are satisfied: GNU General Public License, version 2
// or later which is available at
// https://www.gnu.org/licenses/old-licenses/gpl-2.0-standalone.html
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
/****************************************************************************/
/// @file    GNECrossing.cpp
/// @author  Jakob Erdmann
/// @date    June 2011
///
// A class for visualizing Inner Lanes (used when editing traffic lights)
/****************************************************************************/
#include <config.h>

#include <netedit/GNENet.h>
#include <netedit/GNEUndoList.h>
#include <netedit/GNEViewNet.h>
#include <netedit/changes/GNEChange_Attribute.h>
#include <utils/gui/div/GLHelper.h>
#include <utils/gui/globjects/GLIncludes.h>
#include <utils/gui/globjects/GUIGLObjectPopupMenu.h>
#include <utils/gui/windows/GUIAppEnum.h>

#include "GNECrossing.h"
#include "GNEJunction.h"
#include "GNEEdge.h"


// ===========================================================================
// method definitions
// ===========================================================================
GNECrossing::GNECrossing(GNEJunction* parentJunction, std::vector<NBEdge*> crossingEdges) :
    GNENetworkElement(parentJunction->getNet(), parentJunction->getNBNode()->getCrossing(crossingEdges)->id,
                      GLO_CROSSING, SUMO_TAG_CROSSING,
    {}, {}, {}, {}, {}, {}, {}, {}),
    myParentJunction(parentJunction),
    myCrossingEdges(crossingEdges) {
    // update centering boundary without updating grid
    updateCenteringBoundary(false);
}


GNECrossing::~GNECrossing() {}


const PositionVector&
GNECrossing::getCrossingShape() const {
    auto crossing = myParentJunction->getNBNode()->getCrossing(myCrossingEdges);
    if (crossing) {
        return (crossing->customShape.size() > 0) ? crossing->customShape : crossing->shape;
    } else {
        throw ProcessError("Crossing doesn't exist");
    }
}


void
GNECrossing::updateGeometry() {
    // rebuild crossing and walking areas form node parent
    auto crossing = myParentJunction->getNBNode()->getCrossing(myCrossingEdges);
    // update crossing geometry
    myCrossingGeometry.updateGeometry(crossing->customShape.size() > 0 ?  crossing->customShape : crossing->shape);
}


Position
GNECrossing::getPositionInView() const {
    // currently unused
    return Position(0, 0);
}


GNEMoveOperation* 
GNECrossing::getMoveOperation(const double shapeOffset) {
    // edit depending if shape is being edited
    if (isShapeEdited()) {
        // get original shape
        const PositionVector originalShape = getCrossingShape();
        // declare shape to move
        PositionVector shapeToMove = originalShape;
        // first check if in the given shapeOffset there is a geometry point
        const Position positionAtOffset = shapeToMove.positionAtOffset2D(shapeOffset);
        // check if position is valid
        if (positionAtOffset == Position::INVALID) {
            return nullptr;
        } else {
            // obtain index
            const int index = originalShape.indexOfClosest(positionAtOffset);
            // declare new index
            int newIndex = index;
            // get snap radius
            const double snap_radius = myNet->getViewNet()->getVisualisationSettings().neteditSizeSettings.crossingGeometryPointRadius;
            // check if we have to create a new index
            if (positionAtOffset.distanceSquaredTo2D(shapeToMove[index]) > (snap_radius * snap_radius)) {
                newIndex = shapeToMove.insertAtClosest(positionAtOffset, true);
            }
            // return move operation for edit shape
            return new GNEMoveOperation(this, originalShape, {index}, shapeToMove, {newIndex});
        }
    } else {
        return nullptr;
    }
}


void 
GNECrossing::removeGeometryPoint(const Position clickedPosition, GNEUndoList* undoList) {
    // edit depending if shape is being edited
    if (isShapeEdited()) {
        // get original shape
        PositionVector shape = getCrossingShape();
        // check shape size
        if (shape.size() > 2) {
            // obtain index
            int index = shape.indexOfClosest(clickedPosition);
            // get snap radius
            const double snap_radius = myNet->getViewNet()->getVisualisationSettings().neteditSizeSettings.crossingGeometryPointRadius;
            // check if we have to create a new index
            if ((index != -1) && shape[index].distanceSquaredTo2D(clickedPosition) < (snap_radius * snap_radius)) {
                // remove geometry point
                shape.erase(shape.begin() + index);
                // commit new shape
                undoList->p_begin("remove geometry point of " + getTagStr());
                undoList->p_add(new GNEChange_Attribute(this, SUMO_ATTR_CUSTOMSHAPE, toString(shape)));
                undoList->p_end();
            }
        }
    }
}


GNEJunction*
GNECrossing::getParentJunction() const {
    return myParentJunction;
}


const std::vector<NBEdge*>&
GNECrossing::getCrossingEdges() const {
    return myCrossingEdges;
}


NBNode::Crossing*
GNECrossing::getNBCrossing() const {
    return myParentJunction->getNBNode()->getCrossing(myCrossingEdges);
}


void
GNECrossing::drawGL(const GUIVisualizationSettings& s) const {
    // declare flag for drawing crossing
    bool drawCrossing = s.drawCrossingsAndWalkingareas;
    // don't draw in supermode data
    if (myNet->getViewNet()->getEditModes().isCurrentSupermodeData()) {
        drawCrossing = false;
    }
    // check shape rotations
    if (myCrossingGeometry.getShapeRotations().empty()) {
        drawCrossing = false;
    }
    // check shape lengths
    if (myCrossingGeometry.getShapeLengths().empty()) {
        drawCrossing = false;
    }
    // check zoom
    if (s.scale < 3.0) {
        drawCrossing = false;
    }
    // continue depending of drawCrossing flag
    if (drawCrossing) {
        // get NBCrossing
        const auto NBCrossing = myParentJunction->getNBNode()->getCrossing(myCrossingEdges);
        // draw crossing checking whether it is not too small if isn't being drawn for selecting
        const double selectionScale = isAttributeCarrierSelected() ? s.selectorFrameScale : 1;
        // set default values
        const double length = 0.5 * selectionScale;
        const double spacing = 1.0 * selectionScale;
        const double halfWidth = NBCrossing->width * 0.5 * selectionScale;
        // get color
        RGBColor crossingColor;
        // first check if we're editing shape
        if (myShapeEdited) {
            crossingColor = s.colorSettings.editShape;
        } else if (drawUsingSelectColor()) {
            crossingColor = s.colorSettings.selectedCrossingColor;
        } else if (!NBCrossing->valid) {
            crossingColor = s.colorSettings.crossingInvalid;
        } else if (NBCrossing->priority) {
            crossingColor = s.colorSettings.crossingPriority;
        } else if (myNet->getViewNet()->getEditModes().isCurrentSupermodeData()) {
            crossingColor = s.laneColorer.getSchemes()[0].getColor(8);
        } else {
            crossingColor = s.colorSettings.crossing;
        }
        // check that current mode isn't TLS
        if (myNet->getViewNet()->getEditModes().networkEditMode != NetworkEditMode::NETWORK_TLS) {
            // push name
            glPushName(getGlID());
            // push layer matrix
            glPushMatrix();
            // translate to front
            myNet->getViewNet()->drawTranslateFrontAttributeCarrier(this, GLO_CROSSING);
            // set color
            GLHelper::setColor(crossingColor);
            // draw depending of selection
            if (s.drawForRectangleSelection || s.drawForPositionSelection) {
                // just drawn a box line
                GLHelper::drawBoxLines(myCrossingGeometry.getShape(), halfWidth);
            } else {
                // push rail matrix
                glPushMatrix();
                // draw on top of of the white area between the rails
                glTranslated(0, 0, 0.1);
                for (int i = 0; i < (int)myCrossingGeometry.getShape().size() - 1; i++) {
                    // push draw matrix
                    glPushMatrix();
                    // translate and rotate
                    glTranslated(myCrossingGeometry.getShape()[i].x(), myCrossingGeometry.getShape()[i].y(), 0.0);
                    glRotated(myCrossingGeometry.getShapeRotations()[i], 0, 0, 1);
                    // draw crossing depending if isn't being drawn for selecting
                    for (double t = 0; t < myCrossingGeometry.getShapeLengths()[i]; t += spacing) {
                        glBegin(GL_QUADS);
                        glVertex2d(-halfWidth, -t);
                        glVertex2d(-halfWidth, -t - length);
                        glVertex2d(halfWidth, -t - length);
                        glVertex2d(halfWidth, -t);
                        glEnd();
                    }
                    // pop draw matrix
                    glPopMatrix();
                }
                // pop rail matrix
                glPopMatrix();
            }
            // draw shape points only in Network supemode
            if (myShapeEdited && s.drawMovingGeometryPoint(selectionScale, s.neteditSizeSettings.crossingGeometryPointRadius) && myNet->getViewNet()->getEditModes().isCurrentSupermodeNetwork()) {
                // color
                const RGBColor darkerColor = crossingColor.changedBrightness(-32);
                // draw geometry points
                GNEGeometry::drawGeometryPoints(s, myNet->getViewNet(), myCrossingGeometry.getShape(), darkerColor, darkerColor, s.neteditSizeSettings.crossingGeometryPointRadius, selectionScale);
                // draw moving hint
                GNEGeometry::drawMovingHint(s, myNet->getViewNet(), myCrossingGeometry.getShape(), darkerColor, s.neteditSizeSettings.crossingGeometryPointRadius, selectionScale);
            }
            // pop layer matrix
            glPopMatrix();
            // pop name
            glPopName();
        }
        // link indices must be drawn in all edit modes if isn't being drawn for selecting
        if (s.drawLinkTLIndex.show && !s.drawForRectangleSelection) {
            drawTLSLinkNo(s, NBCrossing);
        }
        // check if dotted contour has to be drawn (not useful at high zoom)
        if (s.drawDottedContour() || myNet->getViewNet()->isAttributeCarrierInspected(this)) {
            GNEGeometry::drawDottedContourShape(GNEGeometry::DottedContourType::INSPECT, s, myCrossingGeometry.getShape(), halfWidth, selectionScale);
        }
    }
}


void
GNECrossing::drawTLSLinkNo(const GUIVisualizationSettings& s, const NBNode::Crossing* crossing) const {
    // push matrix
    glPushMatrix();
    // move to GLO_Crossing
    glTranslated(0, 0, GLO_CROSSING + 0.5);
    // make a copy of shape
    PositionVector shape = crossing->shape;
    // extrapolate
    shape.extrapolate(0.5); // draw on top of the walking area
    // get link indexes
    const int linkNo = crossing->tlLinkIndex;
    const int linkNo2 = crossing->tlLinkIndex2 > 0 ? crossing->tlLinkIndex2 : linkNo;
    // draw link indexes
    GLHelper::drawTextAtEnd(toString(linkNo2), shape, 0, s.drawLinkTLIndex, s.scale);
    GLHelper::drawTextAtEnd(toString(linkNo), shape.reverse(), 0, s.drawLinkTLIndex, s.scale);
    // push matrix
    glPopMatrix();
}


GUIGLObjectPopupMenu*
GNECrossing::getPopUpMenu(GUIMainWindow& app, GUISUMOAbstractView& parent) {
    GUIGLObjectPopupMenu* ret = new GUIGLObjectPopupMenu(app, parent, *this);
    buildPopupHeader(ret, app);
    buildCenterPopupEntry(ret);
    buildNameCopyPopupEntry(ret);
    // build selection and show parameters menu
    myNet->getViewNet()->buildSelectionACPopupEntry(ret, this);
    buildShowParamsPopupEntry(ret);
    // build position copy entry
    buildPositionCopyEntry(ret, false);
    // check if we're in supermode network
    if (myNet->getViewNet()->getEditModes().isCurrentSupermodeNetwork()) {
        // create menu commands
        FXMenuCommand* mcCustomShape = new FXMenuCommand(ret, "Set custom crossing shape", nullptr, &parent, MID_GNE_CROSSING_EDIT_SHAPE);
        // check if menu commands has to be disabled
        NetworkEditMode editMode = myNet->getViewNet()->getEditModes().networkEditMode;
        if ((editMode == NetworkEditMode::NETWORK_CONNECT) || (editMode == NetworkEditMode::NETWORK_TLS) || (editMode == NetworkEditMode::NETWORK_CREATE_EDGE)) {
            mcCustomShape->disable();
        }
    }
    return ret;
}


void
GNECrossing::updateCenteringBoundary(const bool /*updateGrid*/) {
    auto crossing = myParentJunction->getNBNode()->getCrossing(myCrossingEdges);
    if (crossing) {
        if (crossing->customShape.size() > 0) {
            myBoundary = crossing->customShape.getBoxBoundary();
            myBoundary.grow(10);
        } else if (crossing->shape.size() > 0) {
            myBoundary = crossing->shape.getBoxBoundary();
            myBoundary.grow(10);
        } else {
            myBoundary = myParentJunction->getCenteringBoundary();
        }
    } else {
        // in other case use boundary of parent junction
        myBoundary = myParentJunction->getCenteringBoundary();
    }
}


std::string
GNECrossing::getAttribute(SumoXMLAttr key) const {
    auto crossing = myParentJunction->getNBNode()->getCrossing(myCrossingEdges, (key != SUMO_ATTR_ID));
    switch (key) {
        case SUMO_ATTR_ID:
            // get attribute requires a special case
            if (crossing) {
                return crossing->id;
            } else {
                return "Temporal Unreferenced";
            }
        case SUMO_ATTR_WIDTH:
            return toString(crossing->customWidth);
        case SUMO_ATTR_PRIORITY:
            return crossing->priority ? "true" : "false";
        case SUMO_ATTR_EDGES:
            return toString(crossing->edges);
        case SUMO_ATTR_TLLINKINDEX:
            return toString(crossing->customTLIndex);
        case SUMO_ATTR_TLLINKINDEX2:
            return toString(crossing->customTLIndex2);
        case SUMO_ATTR_CUSTOMSHAPE:
            return toString(crossing->customShape);
        case GNE_ATTR_SELECTED:
            return toString(isAttributeCarrierSelected());
        case GNE_ATTR_PARAMETERS:
            return crossing->getParametersStr();
        default:
            throw InvalidArgument(getTagStr() + " doesn't have an attribute of type '" + toString(key) + "'");
    }
}


void
GNECrossing::setAttribute(SumoXMLAttr key, const std::string& value, GNEUndoList* undoList) {
    if (value == getAttribute(key)) {
        return; //avoid needless changes, later logic relies on the fact that attributes have changed
    }
    switch (key) {
        case SUMO_ATTR_ID:
            throw InvalidArgument("Modifying attribute '" + toString(key) + "' of " + getTagStr() + " isn't allowed");
        case SUMO_ATTR_EDGES:
        case SUMO_ATTR_WIDTH:
        case SUMO_ATTR_PRIORITY:
        case SUMO_ATTR_TLLINKINDEX:
        case SUMO_ATTR_TLLINKINDEX2:
        case SUMO_ATTR_CUSTOMSHAPE:
        case GNE_ATTR_SELECTED:
        case GNE_ATTR_PARAMETERS:
            undoList->add(new GNEChange_Attribute(this, key, value), true);
            break;
        default:
            throw InvalidArgument(getTagStr() + " doesn't have an attribute of type '" + toString(key) + "'");
    }
}


bool
GNECrossing::isAttributeEnabled(SumoXMLAttr key) const {
    switch (key) {
        case SUMO_ATTR_ID:
            // id isn't editable
            return false;
        case SUMO_ATTR_TLLINKINDEX:
        case SUMO_ATTR_TLLINKINDEX2:
            return (myParentJunction->getNBNode()->getCrossing(myCrossingEdges)->tlID != "");
        default:
            return true;
    }
}


bool
GNECrossing::isValid(SumoXMLAttr key, const std::string& value) {
    auto crossing = myParentJunction->getNBNode()->getCrossing(myCrossingEdges);
    switch (key) {
        case SUMO_ATTR_ID:
            return false;
        case SUMO_ATTR_EDGES:
            if (canParse<std::vector<GNEEdge*> >(myNet, value, false)) {
                // parse edges and save their IDs in a set
                std::vector<GNEEdge*> parsedEdges = parse<std::vector<GNEEdge*> >(myNet, value);
                EdgeVector nbEdges;
                for (auto i : parsedEdges) {
                    nbEdges.push_back(i->getNBEdge());
                }
                std::sort(nbEdges.begin(), nbEdges.end());
                //
                EdgeVector originalEdges = crossing->edges;
                std::sort(originalEdges.begin(), originalEdges.end());
                // return true if we're setting the same edges
                if (toString(nbEdges) == toString(originalEdges)) {
                    return true;
                } else {
                    return !myParentJunction->getNBNode()->checkCrossingDuplicated(nbEdges);
                }
            } else {
                return false;
            }
        case SUMO_ATTR_WIDTH:
            return canParse<double>(value) && ((parse<double>(value) > 0) || (parse<double>(value) == -1)); // kann NICHT 0 sein, oder -1 (bedeutet default)
        case SUMO_ATTR_PRIORITY:
            return canParse<bool>(value);
        case SUMO_ATTR_TLLINKINDEX:
        case SUMO_ATTR_TLLINKINDEX2:
            // -1 means that tlLinkIndex2 takes on the same value as tlLinkIndex when setting idnices
            return (isAttributeEnabled(key) &&
                    canParse<int>(value)
                    && ((parse<double>(value) >= 0) || ((parse<double>(value) == -1) && (key == SUMO_ATTR_TLLINKINDEX2)))
                    && myParentJunction->getNBNode()->getControllingTLS().size() > 0
                    && (*myParentJunction->getNBNode()->getControllingTLS().begin())->getMaxValidIndex() >= parse<int>(value));
        case SUMO_ATTR_CUSTOMSHAPE: {
            // empty shapes are allowed
            return canParse<PositionVector>(value);
        }
        case GNE_ATTR_SELECTED:
            return canParse<bool>(value);
        case GNE_ATTR_PARAMETERS:
            return Parameterised::areParametersValid(value);
        default:
            throw InvalidArgument(getTagStr() + " doesn't have an attribute of type '" + toString(key) + "'");
    }
}


bool
GNECrossing::checkEdgeBelong(GNEEdge* edge) const {
    auto crossing = myParentJunction->getNBNode()->getCrossing(myCrossingEdges);
    if (std::find(crossing->edges.begin(), crossing->edges.end(), edge->getNBEdge()) !=  crossing->edges.end()) {
        return true;
    } else {
        return false;
    }
}


bool
GNECrossing::checkEdgeBelong(const std::vector<GNEEdge*>& edges) const {
    for (auto i : edges) {
        if (checkEdgeBelong(i)) {
            return true;
        }
    }
    return false;
}

// ===========================================================================
// private
// ===========================================================================

void
GNECrossing::setAttribute(SumoXMLAttr key, const std::string& value) {
    auto crossing = myParentJunction->getNBNode()->getCrossing(myCrossingEdges);
    switch (key) {
        case SUMO_ATTR_ID:
            throw InvalidArgument("Modifying attribute '" + toString(key) + "' of " + getTagStr() + " isn't allowed");
        case SUMO_ATTR_EDGES: {
            // obtain GNEEdges
            std::vector<GNEEdge*> edges = parse<std::vector<GNEEdge*> >(myNet, value);
            // remove NBEdges of crossing
            crossing->edges.clear();
            // set NBEdge of every GNEEdge into Crossing Edges
            for (auto i : edges) {
                crossing->edges.push_back(i->getNBEdge());
            }
            // sort new edges
            std::sort(crossing->edges.begin(), crossing->edges.end());
            // change myCrossingEdges by the new edges
            myCrossingEdges = crossing->edges;
            // update geometry of parent junction
            myParentJunction->updateGeometry();
            break;
        }
        case SUMO_ATTR_WIDTH:
            // Change width an refresh element
            crossing->customWidth = parse<double>(value);
            // update boundary
            updateCenteringBoundary(false);
            break;
        case SUMO_ATTR_PRIORITY:
            crossing->priority = parse<bool>(value);
            break;
        case SUMO_ATTR_TLLINKINDEX:
            crossing->customTLIndex = parse<int>(value);
            // make new value visible immediately
            crossing->tlLinkIndex = crossing->customTLIndex;
            break;
        case SUMO_ATTR_TLLINKINDEX2:
            crossing->customTLIndex2 = parse<int>(value);
            // make new value visible immediately
            crossing->tlLinkIndex2 = crossing->customTLIndex2;
            break;
        case SUMO_ATTR_CUSTOMSHAPE: {
            // set custom shape
            crossing->customShape = parse<PositionVector>(value);
            // update boundary
            updateCenteringBoundary(false);
            break;
        }
        case GNE_ATTR_SELECTED:
            if (parse<bool>(value)) {
                selectAttributeCarrier();
            } else {
                unselectAttributeCarrier();
            }
            break;
        case GNE_ATTR_PARAMETERS:
            crossing->setParametersStr(value);
            break;
        default:
            throw InvalidArgument(getTagStr() + " doesn't have an attribute of type '" + toString(key) + "'");
    }
    // Crossing are a special case and we need ot update geometry of junction instead of crossing
    if ((key != SUMO_ATTR_ID) && (key != GNE_ATTR_PARAMETERS) && (key != GNE_ATTR_SELECTED)) {
        myParentJunction->updateGeometry();
    }
}


void
GNECrossing::setMoveShape(const GNEMoveResult& moveResult) {
    // set custom shape
    myParentJunction->getNBNode()->getCrossing(myCrossingEdges)->customShape = moveResult.shapeToUpdate;
    // update geometry
    updateGeometry();
}


void 
GNECrossing::commitMoveShape(const GNEMoveResult& moveResult, GNEUndoList* undoList) {
    // commit new shape
    undoList->p_begin("moving " + toString(SUMO_ATTR_CUSTOMSHAPE) + " of " + getTagStr());
    undoList->p_add(new GNEChange_Attribute(this, SUMO_ATTR_CUSTOMSHAPE, toString(moveResult.shapeToUpdate)));
    undoList->p_end();
}

/****************************************************************************/
