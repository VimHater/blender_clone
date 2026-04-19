#include "timeline.h"

void timeline_init(Timeline *tl) {
    tl->currentFrame = 0;
    tl->startFrame = 0;
    tl->endFrame = 250;
    tl->fps = 24.0f;
    tl->state = PLAYBACK_STOPPED;
    tl->lastTickTime = 0.0;
}

void timeline_update(Timeline *tl) {
    if (tl->state != PLAYBACK_PLAYING) return;
    double now = GetTime();
    double frameDur = 1.0 / tl->fps;
    // don't auto-advance frames when unlimited (scripts use real time instead)
    if (tl->endFrame >= 999999) return;

    if (now - tl->lastTickTime >= frameDur) {
        tl->currentFrame++;
        if (tl->currentFrame > tl->endFrame) {
            tl->currentFrame = tl->startFrame;
        }
        tl->lastTickTime = now;
    }
}

void timeline_play(Timeline *tl) {
    tl->state = PLAYBACK_PLAYING;
    tl->lastTickTime = GetTime();
}

void timeline_pause(Timeline *tl) {
    tl->state = PLAYBACK_PAUSED;
}

void timeline_stop(Timeline *tl) {
    tl->state = PLAYBACK_STOPPED;
    tl->currentFrame = tl->startFrame;
}

void timeline_set_frame(Timeline *tl, int frame) {
    tl->currentFrame = frame;
    if (tl->currentFrame < tl->startFrame) tl->currentFrame = tl->startFrame;
    if (tl->endFrame < 999999 && tl->currentFrame > tl->endFrame) tl->currentFrame = tl->endFrame;
}

// ---- Keyframes ----

void keyframe_insert(SceneObject *obj, int frame) {
    for (int i = 0; i < obj->keyframeCount; i++) {
        if (obj->keyframes[i].frame == frame) {
            obj->keyframes[i].transform = obj->transform;
            return;
        }
    }
    if (obj->keyframeCount >= MAX_KEYFRAMES) return;
    Keyframe kf;
    kf.frame = frame;
    kf.transform = obj->transform;
    obj->keyframes[obj->keyframeCount++] = kf;

    // insertion sort last element
    for (int i = obj->keyframeCount - 1; i > 0; i--) {
        if (obj->keyframes[i].frame < obj->keyframes[i - 1].frame) {
            Keyframe tmp = obj->keyframes[i];
            obj->keyframes[i] = obj->keyframes[i - 1];
            obj->keyframes[i - 1] = tmp;
        }
    }
}

void keyframe_remove(SceneObject *obj, int frame) {
    for (int i = 0; i < obj->keyframeCount; i++) {
        if (obj->keyframes[i].frame == frame) {
            for (int j = i; j < obj->keyframeCount - 1; j++) {
                obj->keyframes[j] = obj->keyframes[j + 1];
            }
            obj->keyframeCount--;
            return;
        }
    }
}

void keyframe_sync(SceneObject *obj, int frame) {
    for (int i = 0; i < obj->keyframeCount; i++) {
        if (obj->keyframes[i].frame == frame) {
            obj->keyframes[i].transform = obj->transform;
            return;
        }
    }
}

void keyframe_evaluate(const SceneObject *obj, int frame, ObjectTransform *out) {
    if (obj->keyframeCount == 0) {
        *out = obj->transform;
        return;
    }
    if (frame <= obj->keyframes[0].frame) {
        *out = obj->keyframes[0].transform;
        return;
    }
    if (frame >= obj->keyframes[obj->keyframeCount - 1].frame) {
        *out = obj->keyframes[obj->keyframeCount - 1].transform;
        return;
    }
    for (int i = 0; i < obj->keyframeCount - 1; i++) {
        if (frame >= obj->keyframes[i].frame && frame <= obj->keyframes[i + 1].frame) {
            float t = (float)(frame - obj->keyframes[i].frame) /
                      (float)(obj->keyframes[i + 1].frame - obj->keyframes[i].frame);
            *out = transform_lerp(obj->keyframes[i].transform, obj->keyframes[i + 1].transform, t);
            return;
        }
    }
    *out = obj->transform;
}
