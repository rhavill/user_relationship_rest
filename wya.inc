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
    if (!$rels || !is_array($rels)) {
      throw new Exception("User relationship load failed");
    }
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
    return services_error(t('Error getting user relationships.', array()));
  }  
}

function wya_approve($rid) {
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
    return services_error(t('Error approving relationship', array()));
  }
}

function wya_delete($rid) {
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
    return services_error(t('Error removing relationship.', array()));
  }
}

function wya_request($uid) {
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
    return services_error(t('Error requesting relationship.', array()));
  }
}

function wya_request_by_email($email) {
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
    return services_error(t('Error requesting relationship by email.', array()));
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
      return services_error(t('Unable to get location for id @uid.', array('@uid' => $uid)), 403);
    }
  }
  if ($uid) {
    try {
      $query = db_select('wya_location', 'l');
      $row = $query->fields('l', array('latitude', 'longitude', 'accuracy', 'updated'))
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
  return FALSE;
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