#include "say.h"

typedef struct {
  GLuint       id;
  say_image   *img;
  say_context *ctxt;
} say_fbo;

static say_context *say_image_target_make_context(void *data) {
  return say_context_create();
}

static void say_fbo_delete_current(void *data) {
  say_fbo *fbo = (say_fbo*)data;

  if (fbo->ctxt == say_context_current() && fbo->id) {
    glDeleteFramebuffers(1, &fbo->id);
  }
}

void say_fbo_make_current(GLuint fbo);
void say_rbo_make_current(GLuint rbo);

static void say_fbo_build(say_image_target *target, say_fbo *fbo) {
  if (!fbo->id)
    glGenFramebuffers(1, &fbo->id);

  say_fbo_make_current(fbo->id);

  say_image_bind(target->img);
  glGenerateMipmap(GL_TEXTURE_2D);

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D, target->img->texture, 0);

  say_rbo_make_current(target->rbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT,
                        say_image_get_width(target->img),
                        say_image_get_height(target->img));

  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, target->rbo);

  fbo->img = target->img;
}

void say_fbo_make_current(GLuint fbo) {
  say_context *context = say_context_current();

  if (context->fbo != fbo) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    context->fbo = fbo;
  }
}

void say_rbo_make_current(GLuint rbo) {
  say_context *context = say_context_current();

  if (context->rbo != rbo) {
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    context->rbo = rbo;
  }
}

void say_image_target_will_delete(mo_hash *fbos, GLuint rbo) {
  mo_array *contexts = say_context_get_all();

  for (size_t i = 0; i < contexts->size; i++) {
    say_context *context = mo_array_get_as(contexts, i, say_context*);

    say_fbo *fbo = mo_hash_get_ptr(fbos, &context, say_fbo);
    if (fbo && fbo->id == context->fbo)
      context->fbo = 0;

    if (context->rbo == rbo)
      context->rbo = 0;
  }
}

bool say_image_target_is_available() {
  say_context_ensure();
  return GLEW_EXT_framebuffer_object || GLEW_VERSION_3_0;
}

say_image_target *say_image_target_create() {
  say_context_ensure();
  say_image_target *target = malloc(sizeof(say_image_target));

  target->target = say_target_create();
  target->img    = NULL;

  target->fbos = mo_hash_create(sizeof(say_context*), sizeof(say_fbo));
  target->fbos->release = say_fbo_delete_current;
  target->fbos->hash_of = mo_hash_of_pointer;
  target->fbos->key_cmp = mo_hash_pointer_cmp;

  glGenRenderbuffers(1, &target->rbo);

  return target;
}

void say_image_target_free(say_image_target *target) {
  say_context_ensure();
  say_image_target_will_delete(target->fbos, target->rbo);

  glDeleteRenderbuffers(1, &target->rbo);
  mo_hash_free(target->fbos);

  say_target_free(target->target);
  free(target);
}

void say_image_target_set_image(say_image_target *target, say_image *image) {
  say_context_ensure();
  target->img = image;

  if (target->img) {
    say_target_set_custom_data(target->target, target);
    say_target_set_context_proc(target->target, say_image_target_make_context);
    say_target_set_bind_hook(target->target, (say_bind_hook)say_image_target_bind);

    say_vector2 size = say_image_get_size(image);

    say_target_set_size(target->target, size);
    say_view_set_size(target->target->view, size);
    say_view_set_center(target->target->view, say_make_vector2(size.x / 2.0,
                                                               size.y / 2.0));

    say_target_make_current(target->target);
  }
}

say_image *say_image_target_get_image(say_image_target *target) {
  return target->img;
}

void say_image_target_update(say_image_target *target) {
  if (target->img) {
    say_target_update(target->target);
    say_image_mark_out_of_date(target->img);
  }
}

void say_image_target_bind(say_image_target *target) {
  say_context_ensure();

  /*
   * As FBOs aren't shared, we need to fetch the FBO for the current context. If
   * we don't find one, we need to build it.
   */
  say_context *ctxt = say_context_current();

  if (!mo_hash_has_key(target->fbos, &ctxt)) {
    say_fbo tmp = {0, NULL, NULL};
    mo_hash_set(target->fbos, &ctxt, &tmp);

    say_fbo_build(target, mo_hash_get(target->fbos, &ctxt));
  }
  else {
    say_fbo *fbo = mo_hash_get(target->fbos, &ctxt);

    if (fbo->img != target->img && target->img)
      say_fbo_build(target, fbo);
    else
      say_fbo_make_current(fbo->id);
  }

  /*
   * Needed to avoid having garbage data there.
   */
  glClear(GL_DEPTH_BUFFER_BIT);
}

void say_image_target_unbind() {
  if (say_image_target_is_available())
    say_fbo_make_current(0);
}
