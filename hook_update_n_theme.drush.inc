<?php

/**
 * @file
 *
 * Adds a drush command to list users.
 */


 /**
 * Implements hook_drush_command().
 */
function hook_update_n_theme_drush_command() {

  $items['update-n-theme'] = array(
    'description' => 'Run theme updates.',
    'aliases' => array('uptdb'),
    'callback' => 'drush_update_n_theme',
    'options' => array(
      'only-default' => dt("Update only the default enabled theme and no others.")
    ),
    'examples' => array(
      'drush update-n-theme --only-default' => 'Update themes.',
    ),
    'core' => array(6, 7),
    'aliases' => array('uptdb'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
  );

  return $items;
}

 /**
  * Displays a list of users.
  */
function drush_update_n_theme() {

  // A couple of requirements...
  drush_include_engine('drupal', 'update');
  require_once DRUSH_DRUPAL_CORE . '/includes/install.inc';

  // Get our command line option.
  $only_default = drush_get_option("only-default");
  $themes = list_themes();
  $default_theme = variable_get("theme_default");
  $run_themes = array();

  if ($only_default) {
    $run_themes = array($default_theme => $themes[$default_theme]);
  }
  else {
    $run_themes = array_filter($themes, "_filter_disabled_themes");
  }

  $theme_batch = drush_update_n_theme_get_available_updates($run_themes);
  update_main_prepare();
  $sandbox = array();

  // If nothing is available then just quit.
  if (empty($theme_batch)) {
    drush_log("No theme updates available" , "status");
    return;
  }

  // Prompt a confirm is there is something.
  drush_log("Updates are available for the following themes: " . implode(', ', array_keys($theme_batch)), "status");
  $continue = drush_confirm(dt('Are you sure you want to run the updates?'));
  if (empty($continue)) {
    drush_log(dt('Aborting'), 'warning');
    return 0;
  }

  foreach ($theme_batch as $key => $versions) {
    foreach ($versions as $version) {
      $function = $key . "_update_" . $version;
      if (function_exists(($function))) {
        drush_log("Performing: " . $function, "status");
        $results = $function($sandbox);
        if ($results === FALSE) {
          drush_log("FAILED TO RUN: " . $function . ". Function does not exist", "error");
          break;
        }
        else {
          drupal_set_installed_schema_version($key, $version);
        }
      }
      else {
        drush_log("FAILED TO RUN: " . $function . ". Function does not exist", "error");
        break;
      }
    }
  }

  drush_log("Theme update complete.", "ok");
}

/**
 * [drush_update_n_theme_get_available_updates description]
 * @param  [type] $themes [description]
 * @return [type]         [description]
 */
function drush_update_n_theme_get_available_updates($themes = array()) {

  // Get all ...
  if (empty($themes)) {
    $themes = list_themes();
  }

  // Just the theme names.
  $keys = array_keys($themes);
  $available = array();

  // Loop through each and find out what is available.
  foreach ($keys as $theme_name) {
    $updates = drupal_get_theme_schema_versions($theme_name);
    $current = drupal_get_installed_theme_schema_version($theme_name);

    if (!$updates || empty($updates)) {
      continue;
    }

    $remaining = array_filter($updates, function ($x) use ($current) { return (int) $x > $current; });
    if (!empty($remaining)) {
      $available[$theme_name] = $remaining;
    }
  }

  return $available;
}

/**
 * [drupal_get_theme_schema_versions description]
 * @param  [type] $theme [description]
 * @return [type]         [description]
 */
function drupal_get_theme_schema_versions($theme) {
  $updates = &drupal_static(__FUNCTION__, NULL);
  if (!isset($updates[$theme])) {
    $updates = array();

    foreach (array_keys(list_themes()) as $theme_name) {
      $updates[$theme_name] = array();
    }

    // Prepare regular expression to match all possible defined hook_update_N().
    $regexp = '/^(?P<module>.+)_update_(?P<version>\d+)$/';
    $functions = get_defined_functions();
    // Narrow this down to functions ending with an integer, since all
    // hook_update_N() functions end this way, and there are other
    // possible functions which match '_update_'. We use preg_grep() here
    // instead of foreaching through all defined functions, since the loop
    // through all PHP functions can take significant page execution time
    // and this function is called on every administrative page via
    // system_requirements().
    foreach (preg_grep('/_\d+$/', $functions['user']) as $function) {
      // If this function is a module update function, add it to the list of
      // module updates.
      if (preg_match($regexp, $function, $matches)) {
        $updates[$matches['module']][] = $matches['version'];
      }
    }
    // Ensure that updates are applied in numerical order.
    foreach ($updates as &$theme_updates) {
      sort($theme_updates, SORT_NUMERIC);
    }
  }
  return empty($updates[$theme]) ? FALSE : $updates[$theme];
}

/**
 * [drupal_get_installed_theme_schema_version description]
 * @param  [type]  $theme [description]
 * @param  boolean $reset [description]
 * @param  boolean $array [description]
 * @return [type]         [description]
 */
function drupal_get_installed_theme_schema_version($theme, $reset = FALSE, $array = FALSE) {
  static $versions = array();

  if ($reset) {
    $versions = array();
  }

  if (!$versions) {
    $versions = array();
    $result = db_query("SELECT name, schema_version FROM {system} WHERE type = :type", array(':type' => 'theme'));
    foreach ($result as $row) {
      $versions[$row->name] = $row->schema_version;
    }
  }

  if ($array) {
    return $versions;
  }
  else {
    return isset($versions[$theme]) ? $versions[$theme] : SCHEMA_UNINSTALLED;
  }
}


/**
 * Array filter callback to remove disabled themes.
 *
 * @param array $theme
 *   An array of theme information from list_theme.
 *
 * @return bool
 *   True if enabled.
 */
function _filter_disabled_themes($theme) {
  return $theme->status;
}

