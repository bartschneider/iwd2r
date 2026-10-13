"""A small JSON Schema validator for the replay dump schema.

validate(instance, schema) returns a list of error strings, each naming the
JSON path ("$.chargen.abilities[2]"); an empty list means valid. It supports
the subset oracle/replay/schema.json uses: type (one name or a list),
enum, const, required, properties, additionalProperties (a boolean or a
schema), items (one schema for every element), minItems, maxItems,
minimum, maximum, minLength and maxLength. Annotations ($schema, title,
description, and x-* keys) are ignored; any other keyword is an error, so a
schema never relies on a check that is silently skipped.

Standard library only.
"""

ANNOTATIONS = {'$schema', '$id', '$comment', 'title', 'description',
               'default', 'examples'}
KEYWORDS = {'type', 'enum', 'const', 'required', 'properties',
            'additionalProperties', 'items', 'minItems', 'maxItems',
            'minimum', 'maximum', 'minLength', 'maxLength'}


def _is_type(value, name):
    if name == 'null':
        return value is None
    if name == 'boolean':
        return isinstance(value, bool)
    if name == 'integer':
        return isinstance(value, int) and not isinstance(value, bool)
    if name == 'number':
        return isinstance(value, (int, float)) and not isinstance(value, bool)
    if name == 'string':
        return isinstance(value, str)
    if name == 'array':
        return isinstance(value, list)
    if name == 'object':
        return isinstance(value, dict)
    raise ValueError('unknown type %r' % name)


def validate(instance, schema, path='$'):
    """Every way instance breaks schema, as "path: reason" strings."""
    errors = []
    if schema is True:
        return errors
    if schema is False:
        return ['%s: not allowed' % path]
    unknown = sorted(k for k in schema if k not in KEYWORDS
                     and k not in ANNOTATIONS and not k.startswith('x-'))
    if unknown:
        return ['%s: unsupported schema keywords %s' % (path,
                                                        ', '.join(unknown))]
    if 'type' in schema:
        types = schema['type'] if isinstance(schema['type'], list) \
            else [schema['type']]
        if not any(_is_type(instance, t) for t in types):
            return ['%s: %s is not of type %s'
                    % (path, _short(instance), ' or '.join(types))]
    if 'enum' in schema and not any(_equal(instance, v)
                                    for v in schema['enum']):
        errors.append('%s: %s is not one of %s'
                      % (path, _short(instance), schema['enum']))
    if 'const' in schema and not _equal(instance, schema['const']):
        errors.append('%s: %s is not %r' % (path, _short(instance),
                                             schema['const']))
    if _is_type(instance, 'number'):
        if 'minimum' in schema and instance < schema['minimum']:
            errors.append('%s: %s is below the minimum %s'
                          % (path, instance, schema['minimum']))
        if 'maximum' in schema and instance > schema['maximum']:
            errors.append('%s: %s is above the maximum %s'
                          % (path, instance, schema['maximum']))
    if isinstance(instance, str):
        if 'minLength' in schema and len(instance) < schema['minLength']:
            errors.append('%s: shorter than %d characters'
                          % (path, schema['minLength']))
        if 'maxLength' in schema and len(instance) > schema['maxLength']:
            errors.append('%s: longer than %d characters'
                          % (path, schema['maxLength']))
    if isinstance(instance, list):
        if 'minItems' in schema and len(instance) < schema['minItems']:
            errors.append('%s: %d items, fewer than %d'
                          % (path, len(instance), schema['minItems']))
        if 'maxItems' in schema and len(instance) > schema['maxItems']:
            errors.append('%s: %d items, more than %d'
                          % (path, len(instance), schema['maxItems']))
        if 'items' in schema:
            for i, item in enumerate(instance):
                errors += validate(item, schema['items'],
                                   '%s[%d]' % (path, i))
    if isinstance(instance, dict):
        for name in schema.get('required', []):
            if name not in instance:
                errors.append('%s: missing required property %r'
                              % (path, name))
        properties = schema.get('properties', {})
        extra = schema.get('additionalProperties', True)
        for name, value in instance.items():
            where = '%s.%s' % (path, name)
            if name in properties:
                errors += validate(value, properties[name], where)
            elif extra is False:
                errors.append('%s: unexpected property' % where)
            else:
                errors += validate(value, extra, where)
    return errors


def _equal(a, b):
    """JSON equality: 1 and True differ, 1 and 1.0 do not."""
    if isinstance(a, bool) or isinstance(b, bool):
        return type(a) is type(b) and a == b
    return a == b


def _short(value):
    text = repr(value)
    return text if len(text) <= 60 else text[:57] + '...'
