#ifndef CORE_TIMELINE_H
#define CORE_TIMELINE_H

#include "types.h"

struct Timeline {
    int currentFrame;
    int startFrame;
    int endFrame;
    float fps;
    PlaybackState state;
    double lastTickTime;
};

void timeline_init(Timeline *tl);
void timeline_update(Timeline *tl);
void timeline_play(Timeline *tl);
void timeline_pause(Timeline *tl);
void timeline_stop(Timeline *tl);
void timeline_set_frame(Timeline *tl, int frame);

void keyframe_insert(SceneObject *obj, int frame);
void keyframe_remove(SceneObject *obj, int frame);
void keyframe_evaluate(const SceneObject *obj, int frame, ObjectTransform *outTransform);

#endif // CORE_TIMELINE_H
