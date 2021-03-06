<?php
/*
  Copyright (C) 2008 by Phase2 Technology.
  Author(s): Frank Febbraro, Irakli Nadareishvili

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY. See the LICENSE.txt file for more details.

  $Id: calais.ui.inc,v 1.3.2.3.2.12.2.1 2009/03/26 20:54:29 febbraro Exp $
*/
/**
 * @file
 */

define('CALAIS_TERM_MAXLENGTH', 1000);

/**
 * Alter the standard node edit form to remove calais taxonomies so that they are
 * only editable via the Calais tab.
 * TODO: Make this configurable so the user can specify how/where they want to edit calais terms.
 */
function calais_form_alter(&$form, $form_state, $form_id) {

  // Node edit form
  if (isset($form['type']) && $form['type']['#value'] .'_node_form' == $form_id) {
    
    $node = $form['#node'];
    $vocabs = calais_get_vocabularies($node->type);
    
    foreach ($vocabs as $vocabulary) {
      _calais_disable_taxonomy_field($form['taxonomy']['tags'][$vocabulary->vid]);
    }
  }
  // Display Calais GUI on Taxonomy Term Editor if it exists
  else if ($form_id == 'taxonomy_form_term' && !isset($form_state['confirm_delete'])){
    $term = $form['#term'];
    $calais_term = calais_get_term_by_taxonomy($term['tid']);
    $guid = $calais_term->resolved_guid ? $calais_term->resolved_guid : $calais_term->guid;
    if(!empty($guid)) {
      $form['identification']['guid'] = array(
        '#type' => 'markup',
        '#prefix' => '<div class="form-item" id="calais-guid"><label>' .t('Linked Data URI'). '</label>',
        '#value' => l($guid, $guid, array('attributes' => array('target' => '_blank'))),
        '#suffix' => '</div>',
        '#weight' => 10,
      );
    }
  }
}

/**
 * This function implements what is essentially disabling of a taxonomy field, 
 * but leaving it on the form so that values can be recreated by 
 * <code>taxonomy_node_save()</code> upon save.
 */
function _calais_disable_taxonomy_field(&$field) {
  $field['#type'] = 'hidden';
  $field['#maxlength'] = CALAIS_TERM_MAXLENGTH;  
}

/**
 * Render the form for the Calais taxonomy & suggestions
 */
function calais_keywords_form(&$form_state, $node) {
  drupal_set_title(t('Calais Terms: ') . check_plain($node->title));
  
  $path = drupal_get_path('module', 'calais');
  drupal_add_css("$path/calais.css");
  drupal_add_js("$path/calais.js", 'module');

  $vocabs = calais_get_vocabularies($node->type);

  $form          = array();
  $form['#node'] = $node;
  $form['nid']   = array('#type' => 'value', '#value' => $node->nid);
  $form['vid']   = array('#type' => 'value', '#value' => $node->vid);

  uasort($vocabs, '_calais_vocab_cmp');
  $weight = 0;
  foreach ($vocabs as $vocabulary) {
    $vocabulary->weight = $weight++;
    $keywords = calais_get_keywords($node->nid, $node->type, $vocabulary->vid);
    $suggestions = theme('calais_suggestions', $vocabulary->vid, $keywords[$vocabulary->vid]);
    $current_tags = _calais_get_current_tags($node, $vocabulary->vid);
    $has_keywords = sizeof($keywords[$vocabulary->vid]);
    
    // If there are keywords available, make these vocabs appear at the top of the long list
    if ($has_keywords) {
      $vocabulary->weight -= 100;
    }

    // If there are no keywords but existing terms, make these vocabs appear next
    if (!$has_keywords && !empty($current_tags)) {
      $vocabulary->weight -= 50;
    }
    
    $form['taxonomy']['tags'][$vocabulary->vid] = array(
      '#type' => 'textfield',
      '#title' => $vocabulary->name,
      '#description' => $suggestions,
      '#required' => $vocabulary->required,
      '#default_value' => $current_tags,
      '#autocomplete_path' => 'taxonomy/autocomplete/'. $vocabulary->vid,
      '#weight' => $vocabulary->weight,
      '#size' => 75,
      '#maxlength' => CALAIS_TERM_MAXLENGTH,
    );
    
    if ($has_keywords) {
      $form['taxonomy']['tags'][$vocabulary->vid]['#attributes'] = array(
        'class' => 'keywords_available',
      );
    }
  }
  
  $form['taxonomy']['#tree'] = TRUE;
  // Add buttons a top and bottom
  $form['submit1'] = array('#type' => 'submit', '#value' => t('Save'), '#weight' => -200);
  $form['submit2'] = array('#type' => 'submit', '#value' => t('Save'), '#weight' => 200);
  
  return $form;
}

/**
 * Provide validation for the Calais free tagging taxonomy terms.
 *
 * The essence of this was clipped from <code>taxonomy_node_validate()</code>
 */
function calais_keywords_form_validate($form, &$form_state) {
  $node = (object)$form_state['values'];
  taxonomy_node_validate($node);
}

/**
 * Update the node for any Calais keyword modifications.
 */
function calais_keywords_form_submit($form, &$form_state) {
  $node = (object)$form_state['values'];

  // Fill in terms from other non-Calais vocabularies as to not remove them
  $taxonomy = taxonomy_node_get_terms($node);
  $vocabs = calais_get_vocabularies($node->type);
  foreach ($taxonomy as $term){
    if (!isset($vocabs[$term->vid]))
      $node->taxonomy[] = $term;
  }

  taxonomy_node_save($node, $node->taxonomy);

  // Query for Node Terms directly. Some wierd caching (I think) with taxonomy_node_get_terms
  // made it so newly created terms were not returned. This next block of code happens to be
  // VERY interested in newly created terms, so I have to query directly. Boo! :( - ff
  $result = db_query('SELECT t.* FROM {term_node} r 
                      INNER JOIN {term_data} t ON r.tid = t.tid 
                      WHERE r.vid = %d ORDER BY t.weight, t.name', $node->vid);
  while ($term = db_fetch_object($result)) {
    $calais_term = calais_get_node_term_by_name($node->nid, $term->name);
    if($calais_term && $term->vid == $calais_term->vid) {
      if (empty($calais_term->tdid) || $calais_term->tdid != $term->tid) {
        $calais_term->tdid = $term->tid;
        calais_save_term($calais_term);
      }
    }
  }

  drupal_set_message(t('Calais settings have been saved.'));
  $form_state['redirect'] = "node/{$node->nid}";
}

/**
 * Remove term relationship to a node for a particular vocabulary.
 *
 * @param $node The node to remove terms
 * @param $vid The vocabulary whose terms will be removed from the node
 */
function calais_remove_terms_for_node($node, $vid) {
  db_query('DELETE FROM {term_node} WHERE vid = %d AND tid IN (SELECT d.tid FROM {term_data} AS d WHERE d.vid = %d)', $node->vid, $vid);
}

/**
 * Process the node and get all specified terms for the current vocabulary
 */
function _calais_get_current_tags($node, $vid) {
  if (!isset($node->taxonomy))
    return NULL;
  
  $terms = $node->taxonomy;
  $current_tags = taxonomy_implode_tags($terms, $vid);
  $current_tags .= (array_key_exists('tags', $terms) ? $terms['tags'][$vid] : NULL);
  return $current_tags;
}

/**
 * Theme function to render the suggestions for a particular vocabulary
 */
function theme_calais_suggestions($vid, $terms) {
  $score_value = array(0 => 'Low', 1 => 'Low', 2 => 'Moderate', 3 => 'High');
  if (sizeof($terms)) {
    $suggestions .= "<div class='suggestions'>";
    $suggestions .= t('Calais Suggestions: ');
    foreach ($terms as $term) {
      $size = ceil(3 * $term->relevance);
      $hover = t('Relevance: ') . $score_value[$size];
      $suggestions .= "<label class='calais_keyword score-{$size}' for='edit-taxonomy-tags-$vid' title='$hover'>" . check_plain($term->name) . "</label>";
    }
    $suggestions .= "</div>";
  }
  
  return $suggestions;
}

// Vocabulary sorting by name function
function _calais_vocab_cmp($a, $b) {
  return strcmp($a->name, $b->name);
}

