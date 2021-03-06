<?php
/**
 */


function wya_types() {
  return user_relationships_types_load();
}

function wya_mine() {
  return wya_get_friends();
}

function wya_get_friends() {
  try {
    global $user;
    $param = array('user' => $user->uid);
    $options = array('include_user_info' => TRUE);
    $rels = user_relationships_load($param, $options);
    foreach ($rels as $rid => $rel) {
      if (!$rel->approved) {
        unset($rels[$rid]);
      }
      else {
        $friend = new stdClass();
        if ($rel->requester_id == $user->uid) {
          $friend->id = $rel->requestee_id;
          $friend->name = $rel->requestee->name;
        }
        else {
          $friend->id = $rel->requester_id;
          $friend->name = $rel->requester->name;
        }
        $friend->rid = $rid;
        $rels[$rid] = $friend;
      }
    }
    return array_values($rels);
    //return $rels;
  } catch (Exception $ex) {
    return services_error(t('Error getting user relationships.', array()), 500);
  }  
}

function wya_friend_requests() {
  try {
    global $user;
    $param = array('user' => $user->uid);
    $options = array('include_user_info' => TRUE);
    $requests = user_relationships_load($param, $options);
    if (!$requests || !is_array($requests)) {
      return array();
    }
    foreach ($requests as $rid => $request) {
      if ($request->approved) {
        unset($requests[$rid]);
      }
      else {
        $req = new stdClass();
        if ($request->requester_id == $user->uid) {
          $req->requester_id = NULL;
          $req->requestee_id= $request->requestee_id;
          $req->name = $request->requestee->name;
        }
        else {
          $req->requestee_id = NULL;
          $req->requester_id = $request->requester_id;
          $req->name = $request->requester->name;
        }
        $req->rid = $rid;
        $requests[$rid] = $req;
      }
    }
    return array_values($requests);
  } catch (Exception $ex) {
    return services_error(t('Error getting friend requests.', array()), 500);
  }  
}

function wya_approve($rid) {
  try {
    global $user;
    if (!$rid) {
      return services_error(t('Could not approve relationship. Did not receive valid rid.'), array(), 400);
    }
    $rels = user_relationships_load(array('rid' => $rid , 'requestee_id' => $user->uid, 'approved' => 0));
    if (!$rels || !is_array($rels) || count($rels) != 1) {
      return services_error(t('Could not find relationship to approve.'), array(), 404);
    }
    $rel = array_shift($rels);
    if ($rel->requestee_id != $user->uid) {
      return services_error(t('Unauthorized attempt to approve relationship.'), array(), 401);
    }
    user_relationships_save_relationship($rel, 'approve');
    return $rel->rid;
  } catch (Exception $ex) {
    return services_error(t('Error approving relationship', array()), 500);
  }
}

function wya_delete($rid = 0) {
  try {
    global $user;
    if (!$rid) {
      return services_error(t('Could not delete relationship. Did not receive valid rid.'), array(), 400);
    }
    $rels = user_relationships_load(array('rid' => $rid , 'user' => $user->uid));
    if (!$rels || !is_array($rels) || count($rels) != 1) {
      return services_error(t('Could not find relationship to delete.'), array(), 404);
    }
    $rel = array_shift($rels);
    if ($rel->requestee_id != $user->uid && $rel->requester_id != $user->uid) {
      return services_error(t('Unauthorized attempt to delete relationship.'), array(), 401);
    }
    user_relationships_delete_relationship($rel, $user, '');
    return $rel->rid ? $rel->rid : 0;
  } catch (Exception $ex) {
    return services_error(t('Error removing relationship.', array()), 500);
  }
}

/*
 * Delete a friendship when provided with the uid of one of the friends.
 */
function wya_delete_by_uid($params) {
  global $user;
  $friend_uid = $params['uid'];
  try {
    $type = user_relationships_type_load(array('name' => 'Friend'));
    if (!$type) {
      throw new Exception(t('No such relationship type'));
    }
    $rels = user_relationships_load(array('user' => $user->uid));
    if (!$rels || !is_array($rels)) {
      throw new Exception("User relationship load failed");
    }
    foreach ($rels as $rel) {
      if (($rel->requestee_id == $user->uid && $rel->requester_id == $friend_uid) ||
          ($rel->requester_id == $user->uid && $rel->requestee_id == $friend_uid)) {
        user_relationships_delete_relationship($rel, $user, '');
        return $rel->rid;
      }
    }
    return services_error(t('Could not find relationship to delete.', array()), 404);
  } catch (Exception $ex) {
    // this catch captures the results of services_error calls above.
    // Might want to return something more specific than a generic error.
    return services_error(t('Error deleting relationship.', array()), 500);
  }
  
}

function wya_request($uid) {
  $type_name = 'Friend';
  try {
    $type = user_relationships_type_load(array('name' => $type_name));
    //error_log("Relationship request with '$uid' type '$type_name'");
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
    return services_error(t('Error requesting relationship.', array()), 500);
  }
}

function wya_request_by_name_email($params) {
  $type_name = 'Friend';
  global $user;
  $name = $params['name'];
  // First determine if a friend request has already been made by one of the two parties.
  try {
    $param = array('user' => $user->uid);
    $options = array('include_user_info' => TRUE);
    $rels = user_relationships_load($param, $options);
    foreach ($rels as $rid => $relationship) {
      if ($relationship->requestee->name == $name || $relationship->requestee->mail == $name) {
        // a request has already been made, so do nothing.
        return $relationship->rid;
      }
      elseif ($relationship->requester->name == $name || $relationship->requester->mail == $name) {
        // maybe we should automatically create friendship here 
        return $relationship->rid;
      }
    }
  }
  catch (Exception $e) {
    return services_error("Error getting existing friend requests.", 500);
  }
  try {
    $type = user_relationships_type_load(array('name' => $type_name));
    //error_log("Relationship request with '{$user->uid}' type '$type_name'");
    if (!$type) {
      return services_error(t('No such relationship type.'), 500);
    }
    $query = db_select('users', 'u');
    $uid = $query->fields('u', array('uid'))
      ->condition(db_or()
        ->condition('u.name', $name)
        ->condition('u.mail', $name)
      )
      ->condition('u.uid', 0, '>')
      ->execute()
      ->fetchField();
    if (!$uid) {
      return FALSE;
    }    
    $rel = user_relationships_request_relationship($user, $uid, $type);
    if (!$rel) {
      throw new Exception(t('Unknown failure'));
    }
    elseif (!is_object($rel)) {
      throw new Exception($rel);
    }
    return $rel->rid;
  } catch (Exception $ex) {
    return services_error(t('Error requesting relationship by email.', array()), 500);
  }
}

function wya_request_password($name) {
  global $language;
  try {
    $query = db_select('users', 'u');
    $uid = $query->fields('u', array('uid'))
      ->condition(db_or()
        ->condition('u.mail', $name)
        ->condition('u.name', $name)
      )
      ->execute()
      ->fetchField();
  } catch (Exception $ex) {
    return services_error(t('Database error trying to find user with code name or email @name', array('@name' => $name)), 500);
  }
  if (!$uid) {
    return services_error(t('Could not find user with that code name or email.'), 404);
  }    
  $user = user_load($uid);
  // Mail one time login URL and instructions using current language.
  $mail = _user_mail_notify('password_reset', $user, $language);
  if (!empty($mail)) {
    watchdog('user', 'Password reset instructions mailed to %name at %email.', array('%name' => $account->name, '%email' => $account->mail));
    return TRUE;
  }
  return services_error(t('Uncaught error during password request.'), 500);
}

function wya_is_logged_in() {
  $is_logged_in = FALSE;
  global $user;
  if ($user->uid) {
    $is_logged_in = TRUE;
  }
  return $is_logged_in;
}

function wya_retrieve_location($uid) {
  global $user;
  if (!$uid) {
    $uid = $user->uid;
  }
  elseif ($uid != $user->uid) {
    $is_friend = FALSE;
    $friends = wya_get_friends();
    foreach ($friends as $friend) {
      if ($uid == $friend->id) {
        $is_friend = TRUE;
      }
    }
    if (!$is_friend) {
      return services_error(t('Unable to get location for id @uid.', array('@uid' => $uid)), 404);
    }
  }
  if ($uid) {
    try {
      $query = db_select('wya_location', 'l');
      $query->join('users', 'u', 'l.uid = u.uid');
      $row = $query->fields('l', array('latitude', 'longitude', 'accuracy', 'updated'))
        ->fields('u', array('name'))
        ->condition('l.uid', $uid)
        ->execute()
        ->fetchObject();
      if ($row) {
        return $row;        
      }
    }
    catch (Exception $ex) {
      return services_error(t('Database error trying to find location for user @uid', array('@uid' => $uid)), 500);
    }    
  }
  return services_error(t('No location found for id @uid.', array('@uid' => $uid)), 404);
}

function wya_save_location($location) {
  global $user;
  if ($user->uid) {
    try {
      $query = db_select('wya_location', 'l');
      $count = $query->condition('l.uid', $user->uid)
        ->countQuery()
        ->execute()
        ->fetchField();
      if ($count) {
        $query = db_update('wya_location') ;
          $rows_updated = $query->fields(array(
            'latitude' => $location['latitude'],
            'longitude' => $location['longitude'],
            'accuracy' => $location['accuracy'],
            'updated' => $location['updated'],
          ))
          ->condition('uid', $user->uid)
          ->execute();        
      }
      else {
        $query = db_insert('wya_location');
        $query->fields(array(
          'uid' => $user->uid,
          'latitude' => $location['latitude'],
          'longitude' => $location['longitude'],
          'accuracy' => $location['accuracy'],
          'updated' => $location['updated'],
        ))
        ->execute();
      }
    }
    catch (Exception $ex) {
      return services_error(t('Database error trying to save location for user %name', array('%name' => $user->name)), 500);
    }
  }
  return TRUE;
}
