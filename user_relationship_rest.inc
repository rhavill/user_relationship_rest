<?php
/**
 * @file
 * Link general user relationship functionalities to services module.
 *
 * @author Drupal 6 port by Darren Ferguson <http://drupal.org/user/70179>
 * @author Written by scottgifford http://drupal.org/user/245699
 */


function user_relationship_rest_types() {
  return user_relationships_types_load();
}

function user_relationship_rest_mine() {
  try {
    global $user;
    $param = array('user' => $user->uid);
    $options = array('include_user_info' => TRUE);
    $rels = user_relationships_load($param, $options);
xdebug_break();
    if (!$rels || !is_array($rels)) {
      throw new Exception("User relationship load failed");
    }
    foreach ($rels as $rid => $rel) {
      if (!$rel->approved) {
        unset($rels[$rid]);
      }
      else {
        $rel->requester = $rel->requester->name;
        $rel->requestee = $rel->requestee->name;
      }
    }
    return array_values($rels);
  } catch (Exception $ex) {
    return services_error(t('Error getting user relationships: @msg', array('@msg' => $ex->getMessage())));
  }
}

function user_relationship_rest_approve($rid) {
  try {
    global $user;
    $rels = user_relationships_load(array('rid' => $rid , 'requestee_id' => $user->uid, 'approved' => 0));
    if (!$rels || !is_array($rels) || count($rels) != 1) {
      throw new Exception("User relationship load failed");
    }
    $rel = array_shift($rels);
    if ($rel->requestee_id != $user->uid) {
      throw new Exception("Access denied");
    }
    user_relationships_save_relationship($rel, 'approve');
    return $rel;
  } catch (Exception $ex) {
    return services_error(t('Error approving relationship: @msg', array('@msg' => $ex->getMessage())));
  }
}

function user_relationship_rest_delete($rid) {
  try {
    global $user;
    $rels = user_relationships_load(array('rid' => $rid , 'user' => $user->uid));
    if (!$rels || !is_array($rels) || count($rels) != 1) {
      throw new Exception("User relationship load failed");
    }
    $rel = array_shift($rels);
    if ($rel->requestee_id != $user->uid && $rel->requester_id != $user->uid) {
      throw new Exception("Access denied");
    }
    user_relationships_delete_relationship($rel, $user, '');
    return $rel;
  } catch (Exception $ex) {
    return services_error(t('Error removing relationship: @msg', array('@msg' => $ex->getMessage())));
  }
}

function user_relationship_rest_request($uid) {
  $type_name = 'Friend';
  try {
    $type = user_relationships_type_load(array('name' => $type_name));
    error_log("Relationship request with '$uid' type '$type_name'");
    if (!$type) {
      throw new Exception(t('No such relationship type'));
    }

    global $user;
    $ret = user_relationships_request_relationship($user, $uid, $type);
    if (!$ret) {
      throw new Exception(t('Unknown failure'));
    }
    elseif (!is_object($ret)) {
      throw new Exception($ret);
    }
    return $ret;
  } catch (Exception $ex) {
    return services_error(t('Error requesting relationship: @msg', array('@msg' => $ex->getMessage())));
  }
}

function user_relationship_rest_request_by_email($email) {
  $type_name = 'Friend';
  try {
    $type = user_relationships_type_load(array('name' => $type_name));
    error_log("Relationship request with '$uid' type '$type_name'");
    if (!$type) {
      throw new Exception(t('No such relationship type'));
    }
    $query = db_select('users', 'u');
    $uid = $query->fields('u', array('uid'))
      ->condition('u.mail', $email)
      ->condition('u.uid', 0, '>')
      ->execute()
      ->fetchField();
    if (!$uid) {
      return services_error(t('Could not find user with that email.'), 404);
    }    
    global $user;
    $ret = user_relationships_request_relationship($user, $uid, $type);
    if (!$ret) {
      throw new Exception(t('Unknown failure'));
    }
    elseif (!is_object($ret)) {
      throw new Exception($ret);
    }
    return $ret;
  } catch (Exception $ex) {
    return services_error(t('Error requesting relationship by email: @msg', array('@msg' => $ex->getMessage())));
  }
}

function user_relationship_rest_is_logged_in() {
  $is_logged_in = FALSE;
  global $user;
  if ($user->uid) {
    $is_logged_in = TRUE;
  }
  return $is_logged_in;
}
