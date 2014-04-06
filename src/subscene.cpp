#include "Subscene.hpp"
#include "gl2ps.h"
#include "R.h"
#include <algorithm>

using namespace rgl;

//////////////////////////////////////////////////////////////////////////////
//
// CLASS
//   Subscene
//

Subscene::Subscene(Subscene* in_parent, Embedding in_viewport, Embedding in_projection, Embedding in_model)
 : SceneNode(SUBSCENE), parent(in_parent), do_viewport(in_viewport), do_projection(in_projection),
   do_model(in_model), viewport(0.,0.,1.,1.)
{
  viewpoint = NULL;
  bboxdeco   = NULL;
  background = NULL;
  ignoreExtent = false;
  bboxChanges = false;

}

Subscene::~Subscene() 
{
  for (std::vector<Subscene*>::iterator i = subscenes.begin(); i != subscenes.end(); ++ i ) 
    delete (*i);
  if (viewpoint)
    delete viewpoint;
  if (background)
    delete background;
  if (bboxdeco)
    delete bboxdeco;
}

bool Subscene::add(SceneNode* node)
{
  bool success = false;
  switch( node->getTypeID() )
  {
    case SHAPE:
      {
        Shape* shape = (Shape*) node;
        addShape(shape);

        success = true;
      }
      break;
    case LIGHT:
      {
	Light* light = (Light*) node;
	  addLight(light);
	  
	  success = true;
      }
      break;
    case VIEWPOINT:
      {
        if (viewpoint)
          delete viewpoint;
        viewpoint = (Viewpoint*) node;
        success = true;
      }
      break;
    case SUBSCENE:
      {
	Subscene* subscene = static_cast<Subscene*>(node);
	addSubscene(subscene);
	success = true;
      }
      break;
    default:
      break;
  }
  return success;
}

void Subscene::addBackground(Background* newbackground)
{
  if (background)
    delete background;
  background = newbackground;
}

void Subscene::addBboxdeco(BBoxDeco* newbboxdeco)
{
  if (bboxdeco)
    delete bboxdeco;
  bboxdeco = newbboxdeco;
}

void Subscene::addShape(Shape* shape)
{
  if (!shape->getIgnoreExtent()) {
    const AABox& bbox = shape->getBoundingBox();
    data_bbox += bbox;
    bboxChanges |= shape->getBBoxChanges();
  }

  shapes.push_back(shape);
  
  if ( shape->isBlended() ) {
    zsortShapes.push_back(shape);
  } else if ( shape->isClipPlane() ) {
    clipPlanes.push_back(static_cast<ClipPlaneSet*>(shape));
  } else
    unsortedShapes.push_back(shape);
}

void Subscene::addLight(Light* light)
{
  lights.push_back(light);
}

void Subscene::addSubscene(Subscene* subscene)
{
  subscenes.push_back(subscene);
}

void Subscene::pop(TypeID type, int id, bool destroy)
{
  for (std::vector<Subscene*>::iterator i = subscenes.begin(); i != subscenes.end(); ++ i ) 
      (*i)->pop(type, id, destroy);
    
  switch(type) {
    case SHAPE: {    
    
      std::vector<Shape*>::iterator ishape 
      	= std::find_if(shapes.begin(), shapes.end(), 
              std::bind2nd(std::ptr_fun(&sameID), id));
      if (ishape == shapes.end()) return;
        
      Shape* shape = *ishape;
      shapes.erase(ishape);
      if ( shape->isBlended() )
        zsortShapes.erase(std::find_if(zsortShapes.begin(), zsortShapes.end(),
                                       std::bind2nd(std::ptr_fun(&sameID), id)));
      else if ( shape->isClipPlane() )
        clipPlanes.erase(std::find_if(clipPlanes.begin(), clipPlanes.end(),
    		     std::bind2nd(std::ptr_fun(&sameID), id)));
      else
        unsortedShapes.erase(std::find_if(unsortedShapes.begin(), unsortedShapes.end(),
                             std::bind2nd(std::ptr_fun(&sameID), id)));
      
      calcDataBBox();
      break;
    }
    case SUBSCENE: {
      std::vector<Subscene*>::iterator isubscene
	= std::find_if(subscenes.begin(), subscenes.end(),
	  std::bind2nd(std::ptr_fun(&sameID), id));
      if (isubscene == subscenes.end()) return;
      Subscene* subscene = *isubscene;
      subscenes.erase(isubscene);
      if (destroy)
	delete subscene;
      break;
    }
    default: // VIEWPOINT ignored
    break;
  }
}

Subscene* Subscene::get_subscene(int id)
{
  if (id == getObjID()) return this;
    
  for (std::vector<Subscene*>::iterator i = subscenes.begin(); i != subscenes.end() ; ++ i ) {
    Subscene* subscene = (*i)->get_subscene(id);
    if (subscene) return subscene;
  }
  
  return NULL;
}

int Subscene::getAttributeCount(AABox& bbox, AttribID attrib)
{
  switch (attrib) {
    case IDS:	   
    case TYPES:    return shapes.size();
  }
  return SceneNode::getAttributeCount(bbox, attrib);
}

void Subscene::getAttribute(AABox& bbox, AttribID attrib, int first, int count, double* result)
{
  int n = getAttributeCount(bbox, attrib);
  int ind = 0;

  if (first + count < n) n = first + count;
  if (first < n) {
    switch(attrib) {
      case IDS:
        for (std::vector<Shape*>::iterator i = shapes.begin(); i != shapes.end() ; ++ i ) {
      	  if ( first <= ind  && ind < n )  
            *result++ = (*i)->getObjID();
          ind++;
        }
        return;
    }  
    SceneNode::getAttribute(bbox, attrib, first, count, result);
  }
}

String Subscene::getTextAttribute(AABox& bbox, AttribID attrib, int index)
{
  int n = getAttributeCount(bbox, attrib);
  if (index < n && attrib == TYPES) {
    char* buffer = R_alloc(20, 1);    
    shapes[index]->getShapeName(buffer, 20);
    return String(strlen(buffer), buffer);
  } else
    return SceneNode::getTextAttribute(bbox, attrib, index);
}

bool Subscene::clear(TypeID typeID, bool recursive)
{
  bool success = false;
    
  if (recursive) 
    for (std::vector<Subscene*>::iterator i = subscenes.begin(); i != subscenes.end(); ++ i ) 
      (*i)->clear(typeID, true);

  switch(typeID) {
    case SHAPE:
      zsortShapes.clear();
      SAVEGLERROR;
      unsortedShapes.clear();
      SAVEGLERROR;
      clipPlanes.clear();
      SAVEGLERROR;
      bboxChanges = false;      
      success = true;
      break;
    case SUBSCENE:
      subscenes.clear();
      break;
  }
  return success;
}

void Subscene::renderClipplanes(RenderContext* renderContext)
{
  std::vector<ClipPlaneSet*>::iterator iter;
	
  for (iter = clipPlanes.begin() ; iter != clipPlanes.end() ; ++iter ) {
    ClipPlaneSet* plane = *iter;
    plane->render(renderContext);
    SAVEGLERROR;
  }
}

void Subscene::disableClipplanes(RenderContext* renderContext)
{
  std::vector<ClipPlaneSet*>::iterator iter;
	
  for (iter = clipPlanes.begin() ; iter != clipPlanes.end() ; ++iter ) {
    ClipPlaneSet* plane = *iter;
    plane->enable(false);
    SAVEGLERROR;
  }
}
 
int Subscene::get_id_count(TypeID type, bool recursive)
{
  int result = 0;
  if (recursive)
    for (std::vector<Subscene*>::iterator i = subscenes.begin(); i != subscenes.end(); ++ i ) 
      result += (*i)->get_id_count(type);
  switch (type) {
    case SUBSCENE: {
      result += 1;
      break;
    }
    case VIEWPOINT: {    
      result += viewpoint ? 1 : 0;
      break;
    }
    case BACKGROUND: {
      result += background ? 1 : 0;
      break;
    }
    case BBOXDECO: {
      result += bboxdeco ? 1 : 0;
      break;
    }
  }
  return result;
}
    
void Subscene::get_ids(TypeID type, int* ids, char** types, bool recursive)
{
  switch(type) {
  case SUBSCENE: 
    for (std::vector<Subscene*>::iterator i = subscenes.begin(); i != subscenes.end(); ++ i ) {
      *ids++ = (*i)->getObjID();
      *types = R_alloc(strlen("subscene")+1, 1);
      strcpy(*types, "subscene");
      types++;
    }
    break;
  case VIEWPOINT:
    if (viewpoint) {
      *ids = viewpoint->getObjID();
      *types = R_alloc(strlen("viewpoint")+1, 1);
      strcpy(*types, "viewpoint");
      types++;
    }
    break;
  case BBOXDECO:
    if (bboxdeco) {
      *ids = bboxdeco->getObjID();
      *types = R_alloc(strlen("bboxdeco")+1, 1);
      strcpy(*types, "bboxdeco");
      types++;
    }
    break;
  case BACKGROUND:
    if (background) {
      *ids = background->getObjID();
      *types = R_alloc(strlen("background")+1, 1);
      strcpy(*types, "background");
      types++;
    }
    break;
  if (recursive)
    for (std::vector<Subscene*>::iterator i = subscenes.begin(); i != subscenes.end(); ++ i ) {
      (*i)->get_ids(type, ids, types);	
      ids += (*i)->get_id_count(type);
      types += (*i)->get_id_count(type);
    }
  }
}

Background* Subscene::get_background()
{
  if (background) return background;
  else if (parent) return parent->get_background();
  else return NULL;
}

BBoxDeco* Subscene::get_bboxdeco()
{
  if (bboxdeco) return bboxdeco;
  else if (parent) return parent->get_bboxdeco();
  else return NULL;
}

Viewpoint* Subscene::getViewpoint()
{
  if (viewpoint) return viewpoint;
  else if (parent) return parent->getViewpoint();
  else return NULL;
}

void Subscene::render(RenderContext* renderContext)
{
  GLdouble savemodelview[16];
  GLdouble saveprojection[16];
  GLint saveviewport[4];
  
  if (do_viewport > EMBED_INHERIT) {
  
    //
    // SETUP VIEWPORT TRANSFORMATION
    //
    std::copy(std::begin(renderContext->viewport), std::end(renderContext->viewport), std::begin(saveviewport));
    setupViewport(renderContext);
    glGetIntegerv(GL_VIEWPORT, renderContext->viewport);  
  
    SAVEGLERROR;
  }
      
  if (background) {
    GLbitfield clearFlags = background->getClearFlags(renderContext);

    // clear
    glClear(clearFlags);
  }

  if (bboxChanges) 
    calcDataBBox();
  
  Sphere total_bsphere;
  Viewpoint* thisviewpoint = getViewpoint();

  if (data_bbox.isValid()) {
    
    // 
    // GET DATA VOLUME SPHERE
    //

    total_bsphere = Sphere( (bboxdeco) ? bboxdeco->getBoundingBox(data_bbox) : data_bbox, thisviewpoint->scale );
    if (total_bsphere.radius <= 0.0)
      total_bsphere.radius = 1.0;

  } else {
    total_bsphere = Sphere( Vertex(0,0,0), 1 );
  }

  SAVEGLERROR;
  
  // Now render the current scene.  First we use the projection matrix...
  
  if (do_projection > EMBED_INHERIT) {
    
    std::copy(std::begin(renderContext->projection), std::end(renderContext->projection), std::begin(saveprojection));
    setupProjMatrix(renderContext, total_bsphere);
    glGetDoublev(GL_PROJECTION_MATRIX, renderContext->projection);
  
  }
  
  // Now the model matrix
  
  if (do_model > EMBED_INHERIT) {
   
    std::copy(std::begin(renderContext->modelview), std::end(renderContext->modelview), std::begin(savemodelview));
    setupModelMatrix(renderContext, total_bsphere);
    glGetDoublev(GL_MODELVIEW_MATRIX, renderContext->modelview);
  
  }
  
  setupLights(renderContext);
  
  if (background) {
    //
    // RENDER BACKGROUND
    //

    // DISABLE Z-BUFFER TEST
    glDisable(GL_DEPTH_TEST);

    // DISABLE Z-BUFFER FOR WRITING
    glDepthMask(GL_FALSE);
  
    background->render(renderContext);
    SAVEGLERROR;
  }
  
  //
  // RENDER MODEL
  //

  if (data_bbox.isValid() ) {

    //
    // SETUP VIEWPOINT TRANSFORMATION
    //

    // FIXME THIS SHOULD ALREADY BE DONE thisviewpoint->setupTransformation( renderContext, total_bsphere);

    //
    // RENDER SOLID SHAPES
    //

    // ENABLE Z-BUFFER TEST 
    glEnable(GL_DEPTH_TEST);

    // ENABLE Z-BUFFER FOR WRITING
    glDepthMask(GL_TRUE);

    // DISABLE BLENDING
    glDisable(GL_BLEND);
    
    //
    // RENDER BBOX DECO
    //

    if (bboxdeco) 
      bboxdeco->render(renderContext);  // This changes the modelview/projection/viewport

    SAVEGLERROR;

    renderUnsorted(renderContext);

// #define NO_BLEND

#ifndef NO_BLEND
    //
    // RENDER BLENDED SHAPES
    //
    // render shapes in bounding-box sorted order according to z value
    //

    // DISABLE Z-BUFFER FOR WRITING
    glDepthMask(GL_FALSE);
    
    SAVEGLERROR;
    
    // SETUP BLENDING
    if (renderContext->gl2psActive == GL2PS_NONE) 
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    else
      gl2psBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

    SAVEGLERROR;
    
    // ENABLE BLENDING
    glEnable(GL_BLEND);

    SAVEGLERROR;

    //
    // GET THE TRANSFORMATION
    //

    thisviewpoint->setupTransformation(renderContext, total_bsphere);

    Matrix4x4 M(renderContext->modelview);    
    Matrix4x4 P(renderContext->projection);
    P = P*M;
    
    renderContext->Zrow = P.getRow(2);
    renderContext->Wrow = P.getRow(3);
    
    renderZsort(renderContext);
#endif    

    /* Reset flag(s) now that scene has been rendered */
    renderContext->viewpoint->scaleChanged = false;
    
    SAVEGLERROR;
  }
  
  // Render subscenes
    
  std::vector<Subscene*>::const_iterator iter;
  for(iter = subscenes.begin(); iter != subscenes.end(); ++iter) 
    (*iter)->render(renderContext);
    
  if (do_model > EMBED_INHERIT) {
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixd(savemodelview);
  }
  
  if (do_projection > EMBED_INHERIT) {
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixd(saveprojection);
  }
  
}

void Subscene::calcDataBBox()
{
  data_bbox.invalidate();

  std::vector<Shape*>::const_iterator iter;

  bboxChanges = false;
  for(iter = shapes.begin(); iter != shapes.end(); ++iter) {
    Shape* shape = *iter;

    if (!shape->getIgnoreExtent()) {
      data_bbox += shape->getBoundingBox(this);
      bboxChanges |= shape->getBBoxChanges();
    }
  }
}

// ---------------------------------------------------------------------------
void Subscene::setIgnoreExtent(int in_ignoreExtent)
{
  ignoreExtent = (bool)in_ignoreExtent;
}

void Subscene::setupViewport(RenderContext* rctx)
{
  Rect2 rect(0,0,0,0);
  if (do_viewport == EMBED_REPLACE) {
    rect.x = rctx->rect.x + viewport.x*rctx->rect.width;
    rect.y = rctx->rect.y + viewport.y*rctx->rect.height;
    rect.width = rctx->rect.width*viewport.width;
    rect.height = rctx->rect.width*viewport.height;
  } else {
    rect.x = rctx->viewport[0] + viewport.x*rctx->viewport[2];
    rect.y = rctx->viewport[1] + viewport.y*rctx->viewport[3];
    rect.width = rctx->viewport[2]*viewport.width;
    rect.height = rctx->viewport[3]*viewport.height;
  }
  
  glViewport(rect.x, rect.y, rect.width, rect.height);
}

void Subscene::setupProjMatrix(RenderContext* rctx, const Sphere& viewSphere)
{
  glMatrixMode(GL_PROJECTION);
  if (do_projection == EMBED_REPLACE)
    glLoadIdentity();
    
  rctx->viewpoint->setupFrustum(rctx, viewSphere);
    
  SAVEGLERROR;    
}

void Subscene::setupModelMatrix(RenderContext* rctx, const Sphere& viewSphere)
{
  glMatrixMode(GL_MODELVIEW);
  if (do_model == EMBED_REPLACE)
    glLoadIdentity();
    
  rctx->viewpoint->setupTransformation(rctx, viewSphere);
  
  SAVEGLERROR;
}

void Subscene::disableLights(RenderContext* rctx)
{
    
  //
  // disable lights; setup will enable them
  //

  for (int i=0;i<8;i++)
    glDisable(GL_LIGHT0 + i);  
}  

void Subscene::setupLights(RenderContext* rctx) 
{  
  bool anyviewpoint = false;
  std::vector<Light*>::const_iterator iter;

  for(iter = lights.begin(); iter != lights.end() ; ++iter ) {

    Light* light = *iter;

    if (!light->viewpoint)
      light->setup(rctx);
    else
      anyviewpoint = true;
  }

  SAVEGLERROR;

  if (anyviewpoint) {
    //
    // viewpoint lights
    //

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    for(iter = lights.begin(); iter != lights.end() ; ++iter ) {

      Light* light = *iter;

      if (light->viewpoint)
        light->setup(rctx);
    }
    glPopMatrix();
  }
  SAVEGLERROR;

}

void Subscene::renderUnsorted(RenderContext* renderContext)
{
  std::vector<Shape*>::iterator iter;

  for (iter = unsortedShapes.begin() ; iter != unsortedShapes.end() ; ++iter ) {
    Shape* shape = *iter;
    shape->render(renderContext);
    SAVEGLERROR;
  }
}
    
void Subscene::renderZsort(RenderContext* renderContext)
{  
  std::vector<Shape*>::iterator iter;
  std::multimap<float, ShapeItem*> distanceMap;
  int index = 0;

  for (iter = zsortShapes.begin() ; iter != zsortShapes.end() ; ++iter ) {
    Shape* shape = *iter;
    shape->renderBegin(renderContext);
    for (int j = 0; j < shape->getElementCount(); j++) {
	ShapeItem* item = new ShapeItem(shape, j);
	float distance = renderContext->getDistance( shape->getElementCenter(j) );
      distanceMap.insert( std::pair<const float,ShapeItem*>(-distance, item) );
      index++;
    }
  }

  {
    Shape* prev = NULL;
    std::multimap<float,ShapeItem*>::iterator iter;
    for (iter = distanceMap.begin() ; iter != distanceMap.end() ; ++ iter ) {
      ShapeItem* item = iter->second;
      Shape* shape = item->shape;
      if (shape != prev) {
        if (prev) prev->drawEnd(renderContext);
        shape->drawBegin(renderContext);
        prev = shape;
      }
      shape->drawElement(renderContext, item->itemnum);
    }
    if (prev) prev->drawEnd(renderContext);
  }
}

const AABox& Subscene::getBoundingBox()
{ 
  if (bboxChanges) 
      calcDataBBox();
  return data_bbox; 
}