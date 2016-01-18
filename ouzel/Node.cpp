// Copyright (C) 2015 Elviss Strazdins
// This file is part of the Ouzel engine.

#include "Node.h"
#include "Engine.h"
#include "SceneManager.h"
#include "Layer.h"
#include "Animator.h"
#include "Camera.h"
#include "Utils.h"

namespace ouzel
{
    Node::Node()
    {
        
    }

    Node::~Node()
    {
        
    }

    void Node::draw()
    {
        if (_transformDirty)
        {
            calculateTransform();
        }
    }
    
    void Node::update(float delta)
    {
        if (_currentAnimator)
        {
            _currentAnimator->update(delta);
        }
        
        if (_transformDirty)
        {
            calculateTransform();
        }
    }
    
    bool Node::addChild(NodePtr const& node)
    {
        if (NodeContainer::addChild(node))
        {
            node->_hasParent = true;
            node->addToLayer(_layer);
            
            if (_transformDirty)
            {
                calculateTransform();
            }
            else
            {
                node->updateTransform(_transform);
            }
            
            return true;
        }
        else
        {
            return false;
        }
    }
    
    bool Node::removeChild(NodePtr const& node)
    {
        if (NodeContainer::removeChild(node))
        {
            node->_hasParent = false;
            node->removeFromLayer();
            node->_layer.reset();
            
            return true;
        }
        else
        {
            return false;
        }
    }

    void Node::setZ(float z)
    {
        _z = z;
        
        if (LayerPtr layer = _layer.lock())
        {
            layer->reorderNodes();
        }
        
        markTransformDirty();
    }

    void Node::setPosition(const Vector2& position)
    {
        _position = position;
        
        markTransformDirty();
    }

    void Node::setRotation(float rotation)
    {
        _rotation = rotation;
        
        markTransformDirty();
    }

    void Node::setScale(const Vector2& scale)
    {
        _scale = scale;
        
        markTransformDirty();
    }
    
    void Node::setFlipX(bool flipX)
    {
        _flipX = flipX;
        
        markTransformDirty();
    }
    
    void Node::setFlipY(bool flipY)
    {
        _flipY = flipY;
        
        markTransformDirty();
    }
    
    void Node::setVisible(bool visible)
    {
        if (visible != _visible)
        {
            _visible = visible;
        }
    }

    void Node::addToLayer(LayerWeakPtr const& layer)
    {
        _layer = layer;
        
        if (LayerPtr layer = _layer.lock())
        {
            layer->addNode(shared_from_this());
            
            for (NodePtr child : _children)
            {
                child->addToLayer(layer);
            }
        }
    }

    void Node::removeFromLayer()
    {
        if (LayerPtr layer = _layer.lock())
        {
            layer->removeNode(shared_from_this());
            
            for (NodePtr child : _children)
            {
                child->removeFromLayer();
            }
        }
    }

    bool Node::pointOn(const Vector2& position) const
    {
        Vector3 localPosition = Vector3(position.x, position.y, 0.0f);
        
        const Matrix4& inverseTransform = getInverseTransform();
        inverseTransform.transformPoint(&localPosition);
        
        return _boundingBox.containPoint(Vector2(localPosition.x, localPosition.y));
    }
    
    bool Node::rectangleOverlaps(const Rectangle& rectangle) const
    {
        if (_boundingBox.isEmpty())
        {
            return false;
        }
        
        Vector3 localLeftBottom = Vector3(rectangle.x, rectangle.y, 0.0f);
        
        const Matrix4& inverseTransform = getInverseTransform();
        inverseTransform.transformPoint(&localLeftBottom);
        
        Vector3 localRightTop =  Vector3(rectangle.x + rectangle.width, rectangle.y + rectangle.height, 0.0f);
        
        inverseTransform.transformPoint(&localRightTop);
        
        if (localLeftBottom.x > _boundingBox.max.x ||
            localLeftBottom.y > _boundingBox.max.y ||
            localRightTop.x < _boundingBox.min.x ||
            localRightTop.y < _boundingBox.min.y)
        {
            return false;
        }
        
        return true;
    }
    
    const Matrix4& Node::getTransform() const
    {
        if (_transformDirty)
        {
            calculateTransform();
        }
        
        return _transform;
    }
    
    const Matrix4& Node::getInverseTransform() const
    {
        if (_transformDirty)
        {
            calculateTransform();
        }
        
        if (_inverseTransformDirty)
        {
            calculateInverseTransform();
        }
        
        return _inverseTransform;
    }

    void Node::updateTransform(const Matrix4& parentTransform)
    {
        _parentTransform = parentTransform;
        calculateTransform();
        _inverseTransformDirty = true;
    }
    
    bool Node::checkVisibility() const
    {
        if (LayerPtr layer = _layer.lock())
        {
            if (_boundingBox.isEmpty())
            {
                return true;
            }
            
            Matrix4 mvp = layer->getProjection() * layer->getCamera()->getTransform() * _transform;
            
            Vector3 corners[4] = {
                Vector3(_boundingBox.min.x, _boundingBox.min.y, 0.0f),
                Vector3(_boundingBox.max.x, _boundingBox.min.y, 0.0f),
                Vector3(_boundingBox.max.x, _boundingBox.max.y, 0.0f),
                Vector3(_boundingBox.min.x, _boundingBox.max.y, 0.0f)
            };
            
            for (Vector3& corner : corners)
            {
                mvp.transformPoint(&corner);
                
                if (corner.x >= -1.0f && corner.x <= 1.0f && corner.y >= -1.0f && corner.y <= 1.0f)
                {
                    return true;
                }
            }
            
            // TODO: handle objects bigger than screen
        }
        
        return false;
    }
    
    void Node::animate(AnimatorPtr const& animator)
    {
        _currentAnimator = animator;
        
        if (_currentAnimator)
        {
            _currentAnimator->start(shared_from_this());
        }
    }
    
    void Node::calculateTransform() const
    {
        Matrix4 translation;
        translation.translate(Vector3(_position.x, _position.y, 0.0f));
        
        Matrix4 rotation;
        rotation.rotate(Vector3(0.0f, 0.0f, -1.0f), _rotation);
        
        Vector3 realScale = Vector3(_scale.x * (_flipX ? -1.0f : 1.0f),
                                    _scale.y * (_flipY ? -1.0f : 1.0f),
                                    1.0f);
        
        Matrix4 scale;
        scale.scale(realScale);
        
        _transform = _parentTransform * translation * rotation * scale;
        _transformDirty = false;
        
        for (NodePtr child : _children)
        {
            child->updateTransform(_transform);
        }
    }
    
    void Node::calculateInverseTransform() const
    {
        if (_transformDirty)
        {
            calculateTransform();
        }
        
        _inverseTransform = _transform;
        _inverseTransform.invert();
        _inverseTransformDirty = false;
    }
    
    void Node::markTransformDirty() const
    {
        _transformDirty = true;
        _inverseTransformDirty = true;
    }
}
